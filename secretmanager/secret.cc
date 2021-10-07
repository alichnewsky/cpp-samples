#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <google/cloud/secretmanager/v1/service.grpc.pb.h>
#include <google/protobuf/util/json_util.h>
#include <grpcpp/grpcpp.h>

using google::cloud::secretmanager::v1::AccessSecretVersionRequest;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cloud::secretmanager::v1::ListSecretsRequest;
using google::cloud::secretmanager::v1::ListSecretsResponse;
using google::cloud::secretmanager::v1::ListSecretVersionsRequest;
using google::cloud::secretmanager::v1::ListSecretVersionsResponse;

using google::cloud::secretmanager::v1::SecretManagerService;

int main(int argc, char** argv){

  bool verbose = false;
  std::string project_numeric_id;

  // FIXME, logger, log level, etc.
  for(int iarg=1; iarg < argc; ++iarg){
    std::string arg( argv[iarg]);
    if ( arg == "-v" || arg == "--verbose"){
      verbose = true;
    }
  }

  google::protobuf::util::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  options.preserve_proto_field_names = true;

  std::cout.flush();
  // FIXME : does grcp wrap this ? do I need to write it with libcpr
  // this does fork exec, with system curl, for a stable endpoint, though.
  std::system("curl -sL -H 'Metadata-Flavor: Google' 'http://169.254.169.254/computeMetadata/v1/project/numeric-project-id' > projectid.txt");
  
  std::ostringstream oss;
  oss <<  std::ifstream("projectid.txt").rdbuf();
  std::system( "rm -rf projectid.txt");

  project_numeric_id = oss.str();


  auto creds = grpc::GoogleDefaultCredentials();
  auto channel = grpc::CreateChannel("secretmanager.googleapis.com", creds);

  std::unique_ptr<SecretManagerService::Stub> secretmanager( SecretManagerService::NewStub(channel));

  ListSecretsRequest request;

  // FIXME: strcat
  std::string parent = "projects/";
  parent += project_numeric_id;
  request.set_parent( std::move( parent) );

  // FIXME: idiomiatic way of doing paged outputs
  // FIXME: idiomatic way to move parts of response protos into the next query
  //        without too much memory allocations.

  grpc::ClientContext ctx;
  
  bool goon = false;
  do {
    ListSecretsResponse response;
    grpc::Status rpc_status = secretmanager->ListSecrets(&ctx, request,  &response);
    
    if ( ! rpc_status.ok() ){
      std::cerr << "ListSecrets failed " << rpc_status.error_message() << std::endl;
      return -1;
    }
    
    int numSecrets = response.secrets_size();
    for ( int i = 0; i < numSecrets; ++i ){
      auto * s = response.mutable_secrets(i);

      if ( verbose ){
	std::string json_string;
	MessageToJsonString(*s, &json_string, options);
	std::cout << json_string << std::endl;
      }

      ListSecretVersionsRequest vrequest;
      ListSecretVersionsResponse vresponse;
      
      vrequest.set_allocated_parent(s->release_name());

      grpc::ClientContext ctx;
      rpc_status = secretmanager->ListSecretVersions( &ctx, vrequest, &vresponse);
      if ( !rpc_status.ok()){
	std::cerr << "ListSecretVersions for " << vrequest.parent() << "  failed with " << rpc_status.error_message() << std::endl;
      } else {
	// paginated as well
	bool goon2 = false;
	do {
	  int numVersions = vresponse.versions_size();
	  for( int v = 0; v < numVersions; ++v ){
	    auto *  version = vresponse.mutable_versions(v);
	    
	    if ( verbose ){
	      std::string json_string;
	      MessageToJsonString( *version, &json_string, options);
	      std::cout << json_string << std::endl;
	    }
	    grpc::ClientContext ctx;
	    AccessSecretVersionRequest arequest;
	    AccessSecretVersionResponse aresponse;
	    
	    arequest.set_allocated_name( version->release_name());
	    
	    rpc_status = secretmanager->AccessSecretVersion( &ctx, arequest, &aresponse);
	    
	    //for this to work your default credentials / service account must have secret accessor permissions.
	    // otherwise it will fail...
	    // either add to default service account, use another service account, ...
	    if ( !rpc_status.ok()){
	      std::cerr << "AccessSecretVersion for " << arequest.name() << "  failed with " << rpc_status.error_message() << std::endl;
	    } else {
	      if ( aresponse.has_payload()){
		if ( verbose ) std::cout << "Payload\n";

		std::cout << aresponse.payload().data() << std::endl;
	      }
	    }
	  }

	  // FIXME : more pagination ?
	} while ( goon2 );
      }
      
    }


    if ( response.next_page_token().empty() == false ){
      request.set_allocated_page_token( response.release_next_page_token());
      goon = true;
    } else {
      goon = false;
    }
  } while ( goon );
  return 0;
}
