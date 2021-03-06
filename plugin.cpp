/*
 * Fledge south plugin.
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <opcua.h>
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>

typedef void (*INGEST_CB)(void *, Reading);

using namespace std;

#define PLUGIN_NAME	"open62541"

/**
 * Default configuration
 */
static const char *default_config = QUOTE({
	"plugin" : {
       		"description" : "Simple OPC UA data change plugin",
		"type" : "string",
	       	"default" : PLUGIN_NAME,
		"readonly" : "true"
		},
	"asset" : {
       		"description" : "Asset name",
		"type" : "string",
	       	"default" : "opcua",
		"displayName" : "Asset Name",
	       	"order" : "1",
	       	"mandatory": "true"
	       	},
	"url" : { "description" : "URL of the OPC UA Server",
		"type" : "string",
	       	"default" : "opc.tcp://mark.local:53530/OPCUA/SimulationServer",
		"displayName" : "OPCUA Server URL",
	       	"order" : "2"},
	"subscription" : {
		"description" : "Variables to observe changes in",
		"type" : "JSON",
	       	"default" : "{ \"subscriptions\" : [  \"ns=5;s=85/0:Simulation\" ] }",
		"displayName" : "OPCUA Object Subscriptions",
	       	"order" : "3"
       		},
	"reportingInterval" : {
		"description" : "The minimum reporting interval for data change notifications" ,
		"type" : "integer",
		"default" : "1000",
		"displayName" : "Min Reporting Interval (millisec)",
		"order" : "5"
		},
	"securityMode" : {
		"description" : "Security mode to use while connecting to OPCUA server" ,
		"type" : "enumeration",
		"options":["Any", "None", "Sign", "SignAndEncrypt"],
		"default" : "Any",
		"displayName" : "Security mode",
		"order" : "6"
		},
	"securityPolicy" : {
		"description" : "Security policy to use while connecting to OPCUA server" ,
		"type" : "enumeration",
		"options":["Any", "Basic256", "Basic256Sha256"],
		"default" : "Any",
		"displayName" : "Security policy",
		"order" : "7",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
		},
	"userAuthPolicy" : {
		"description" : "User authentication policy to use while connecting to OPCUA server" ,
		"type" : "enumeration",
		"options":["anonymous", "username"],
		"default" : "anonymous",
		"displayName" : "User authentication policy",
		"order" : "8"
		},
	"username" : {
		"description" : "Username" ,
		"type" : "string",
		"default" : "",
		"displayName" : "Username",
		"order" : "9",
		"validity": " userAuthPolicy == \"username\" "
		},
	"password" : {
		"description" : "Password" ,
		"type" : "password",
		"default" : "",
		"displayName" : "Password",
		"order" : "10",
		"validity": " userAuthPolicy == \"username\" "
#if CERTIFICATES
		},
	"caCert" : {
		"description" : "CA certificate authority file in DER format" ,
		"type" : "string",
		"default" : "cacert",
		"displayName" : "CA certificate authority",
		"order" : "11",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
		},
	"serverCert" : {
		"description" : "Server certificate in the DER format" ,
		"type" : "string",
		"default" : "OPCUAServer",
		"displayName" : "Server public key",
		"order" : "12",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
		},
	"clientCert" : {
		"description" : "Client public key file in DER format" ,
		"type" : "string",
		"default" : "clientcert",
		"displayName" : "Client public key",
		"order" : "13",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
		},
	"clientKey" : {
		"description" : "Client private key file in DER format" ,
		"type" : "string",
		"default" : "clientkey",
		"displayName" : "Client private key",
		"order" : "14",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
		},
	"caCrl" : {
		"description" : "Certificate Revocation List in DER format" ,
		"type" : "string",
		"default" : "cacrl",
		"displayName" : "Certificate revocation list",
		"order" : "15",
		"validity": " securityMode == \"Sign\" || securityMode == \"SignAndEncrypt\" "
#endif
		}
	});

/**
 * The OPCUA plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
	PLUGIN_NAME,              // Name
	VERSION,                  // Version
	SP_ASYNC, 		  // Flags
	PLUGIN_TYPE_SOUTH,        // Type
	"1.0.0",                  // Interface version
	default_config		  // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	Logger::getLogger()->info("OPC UA Config is %s", info.config);
	return &info;
}

/**
 * Initialise the plugin, called to get the plugin handle
 */
PLUGIN_HANDLE plugin_init(ConfigCategory *config)
{
OPCUA	*opcua;
string	url;


	if (config->itemExists("url"))
	{
		url = config->getValue("url");
		opcua = new OPCUA(url);
	}
	else
	{
		Logger::getLogger()->fatal("UPC UA plugin is missing a URL");
		throw exception();
	}
	opcua->setConfiguration(config);

	return (PLUGIN_HANDLE)opcua;
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle)
{
OPCUA *opcua = (OPCUA *)handle;


	if (!handle)
		return;
	opcua->start();
}

/**
 * Register ingest callback
 */
void plugin_register_ingest(PLUGIN_HANDLE *handle, INGEST_CB cb, void *data)
{
OPCUA *opcua = (OPCUA *)handle;

	if (!handle)
		throw new exception();
	opcua->registerIngest(data, cb);
}

/**
 * Poll for a plugin reading
 */
Reading plugin_poll(PLUGIN_HANDLE *handle)
{
OPCUA *opcua = (OPCUA *)handle;

	throw runtime_error("OPCUA is an async plugin, poll should not be called");
}

/**
 * Reconfigure the plugin
 *
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string& newConfig)
{
ConfigCategory	config("new", newConfig);
OPCUA		*opcua = (OPCUA *)*handle;

	opcua->stop();
	if (config.itemExists("url"))
	{
		string url = config.getValue("url");
		opcua->newURL(url);
	}

	opcua->setConfiguration(&config);

	opcua->start();
	Logger::getLogger()->info("UPC UA plugin restart after reconfigure");
}

/**
 * Shutdown the plugin
 */
void plugin_shutdown(PLUGIN_HANDLE *handle)
{
OPCUA *opcua = (OPCUA *)handle;

	opcua->stop();
	delete opcua;
}
};
