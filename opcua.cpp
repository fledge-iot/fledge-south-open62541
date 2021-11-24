/*
 * Fledge south service plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <opcua.h>
#include <reading.h>
#include <logger.h>
#include <map>

using namespace std;

// Hold subscription variables

/**
 * Wrapper function to call Fledge logger for intern open62541 logger requests
 *
 * @param logContext	instance of open62541 class
 * @param level		The log level
 * @param category	The category of the log message
 * @param msg		The message itself
 * @param args		Argument for the message
 */
void logWrapper(void *logContext, UA_LogLevel level, UA_LogCategory category,
                const char *msg, va_list args)
{
	char buf[200];
	vsnprintf(buf, sizeof(buf), msg, args);
	switch (level)
	{
		case UA_LOGLEVEL_FATAL:
			Logger::getLogger()->fatal(buf);
			break;
		case UA_LOGLEVEL_ERROR:
			Logger::getLogger()->error(buf);
			break;
		case UA_LOGLEVEL_WARNING:
			Logger::getLogger()->warn(buf);
			break;
		case UA_LOGLEVEL_INFO:
			Logger::getLogger()->info(buf);
			break;
		case UA_LOGLEVEL_DEBUG:
		case UA_LOGLEVEL_TRACE:
			Logger::getLogger()->debug(buf);
			break;
	}
}

/**
 * open62541 log plugin clear entry point
 *
 * @param context The context of the logger plugin
 */
void logClear(void *context)
{
	// Do nothing
}

/**
 * Constructor for the opcua plugin
 */
OPCUA::OPCUA(const string& url) : m_url(url), m_subscribeById(false),
	m_connected(false), m_client(NULL)
{
	m_UAlogger.log = logWrapper;
	m_UAlogger.context = this;
	m_UAlogger.clear = logClear;
}

/**
 * Data changed callback for monitored items in the OPCUA server
 */
static void dataChangeHandler(UA_Client *client, UA_UInt32 subId, void *subContext,
                         UA_UInt32 monId, void *monContext, UA_DataValue *value)
{
	OPCUA *opcua = (OPCUA *)subContext;
	const string *nameNode = (string *)monContext;
	opcua->dataChanged(nameNode, value);
}

static void threadWrapper(void *data)
{
	OPCUA *opcua = (OPCUA *)data;
	opcua->threadStart();
}

/**
 * Destructor for the opcua interface
 */
OPCUA::~OPCUA()
{
	if (m_client)
		UA_Client_delete(m_client);
}

/**
 * Set the asset name for the asset we write
 *
 * @param asset Set the name of the asset with insert into readings
 */
void
OPCUA::setAssetName(const std::string& asset)
{
	m_asset = asset;
}

/**
 * Clear down the subscriptions ahead of reconfiguration
 */
void
OPCUA::clearSubscription()
{
	lock_guard<mutex> guard(m_configMutex);
	m_subscriptions.clear();
}

/**
 * Add a subscription parent node to the list
 */
void
OPCUA::addSubscription(const string& parent)
{
	lock_guard<mutex> guard(m_configMutex);
	m_subscriptions.push_back(parent);
}

/**
 * Restart the OPCUA connection
 */
void
OPCUA::restart()
{
	stop();
	start();
}

/**
 * Recurse the object tree and add subscriptions for all variables that are found.
 * The member variable m_subscriptions holds filters that wil be applied to the
 * subscription process. If this is non-empty then it contains a set of strings which
 * are matched against the name of the items in the object tree. Only variables that are in
 * a node that is a desendant of one of these named nodes is added to the subscription list.
 *
 * @param	The node to recurse from
 * @active	Should subscriptions be added, i.e. have we satisfied any filtering requirements.
 * @return	The number of subscriptions added
 */
int OPCUA::addSubscribe(const UA_NodeId *node, bool active)
{
	int n_subscriptions = 0;
	UA_BrowseRequest bReq;
	UA_BrowseRequest_init(&bReq);
	bReq.requestedMaxReferencesPerNode = 0;
	bReq.nodesToBrowse = UA_BrowseDescription_new();
	bReq.nodesToBrowseSize = 1;
	bReq.nodesToBrowse[0].nodeId = *node;
	bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL; /* return everything */
	UA_BrowseResponse bResp = UA_Client_Service_browse(m_client, bReq);
	if (bResp.resultsSize == 0)
		Logger::getLogger()->debug("Browse results in 0 result size");
	for (size_t i = 0; i < bResp.resultsSize; i++)
	{
		for (size_t j = 0; j < bResp.results[i].referencesSize; j++)
		{
			UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
			UA_String str = UA_STRING_NULL;
			UA_NodeId_toString(&(ref->nodeId.nodeId), &str);
			if (ref->nodeClass == UA_NODECLASS_VARIABLE)
			{
				Logger::getLogger()->debug("Node %s is a variable", str.data);
				UA_MonitoredItemCreateRequest monRequest =
					UA_MonitoredItemCreateRequest_default(ref->nodeId.nodeId);
				string *name = new string((char *)str.data);
				// Strip " from name
				size_t pos;
				while ((pos = name->find_first_of("\"")) != std::string::npos)
				{
					name->erase(pos, 1);
				}
				UA_MonitoredItemCreateResult monResponse =
					UA_Client_MonitoredItems_createDataChange(m_client, m_subscriptionId,
                                              UA_TIMESTAMPSTORETURN_BOTH,
                                              monRequest, name, dataChangeHandler, NULL);
				if(monResponse.statusCode != UA_STATUSCODE_GOOD)
				{
					Logger::getLogger()->error("Failed to monitor node %s", name->c_str());
				}
			}
			else if (ref->nodeClass == UA_NODECLASS_OBJECT)
			{
				Logger::getLogger()->debug("Node %s is an object", str.data);
				addSubscribe(&(ref->nodeId.nodeId), active);
			}
		}
	}
	UA_BrowseRequest_clear(&bReq);
	UA_BrowseResponse_clear(&bResp);
	return 0;
}


/**
 * Starts the plugin
 *
 * We register with the OPC UA server, retrieve all the objects under the parent
 * to which we are subscribing and start the process to enable OPC UA to send us
 * change notifications for those items.
 */
void
OPCUA::start()
{
int n_subscriptions = 0;

	m_client = UA_Client_new();
	UA_ClientConfig *config = UA_Client_getConfig(m_client);
	config->securityMode = m_secMode;
	config->securityPolicyUri = UA_STRING_ALLOC((char *)m_secPolicy.c_str());
	config->logger = m_UAlogger;
	UA_ClientConfig_setDefault(config);
	UA_StatusCode rval;
	if (m_authPolicy.compare("username") == 0)
	{
		rval = UA_Client_connectUsername(m_client, m_url.c_str(), m_username.c_str(), m_password.c_str());
		Logger::getLogger()->info("Connecting to %s with username %s/%s and policy '%s'", m_url.c_str(), m_username.c_str(), m_password.c_str(), m_secPolicy.c_str());
	}
	else
	{
		rval = UA_Client_connect(m_client, m_url.c_str());
	}
	if (rval != UA_STATUSCODE_GOOD)
	{
		Logger::getLogger()->fatal("Unable to connect to server %s, %x", UA_StatusCode_name(rval), rval);
		UA_Client_delete(m_client);
		m_client = NULL;
		throw runtime_error("Failed to connect to OPCUA server");
	}
	m_connected = true;

	UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
	UA_CreateSubscriptionResponse response = UA_Client_Subscriptions_create(m_client, request, this, NULL, NULL);
	if (response.responseHeader.serviceResult == UA_STATUSCODE_GOOD)
		m_subscriptionId = response.subscriptionId;
	else
		Logger::getLogger()->error("Failed to create subscription for OPCUA server");

	// Now parse and add the subscriptions
	for (auto item : m_subscriptions)
	{
		Logger::getLogger()->debug("Adding subscriptions for node '%s'", item.c_str());
		UA_NodeId id;
		UA_String str = UA_STRING_ALLOC((char *)item.c_str());
		UA_NodeId_parse(&id, str);
		addSubscribe(&id, true);
	}

	m_threadStop = false;
	m_thread = new thread(threadWrapper, this);
}

void OPCUA::threadStart()
{
	while (! m_threadStop)
	{
		UA_Client_run_iterate(m_client, 1000);
	}
}

/**
 * Stop all subscriptions and disconnect from the OPCUA server
 */
void
OPCUA::stop()
{
	m_threadStop = true;
	if (m_connected)
	{
		m_subscriptions.clear();
		UA_Client_disconnect(m_client);
	}
}

/**
 * Called when a data changed event is received. This calls back to the south service
 * and adds the points to the readings queue to send.
 *
 * @param points	The points in the reading we must create
 */
void OPCUA::ingest(vector<Datapoint *>	points)
{
string asset = m_asset + points[0]->getName();

	(*m_ingest)(m_data, Reading(asset, points));
}


/**
 * Set the message security mode
 *
 * @param value    Security mode string
 */
void
OPCUA::setSecMode(const std::string& secMode)
{ 
	if (secMode.compare("Any") == 0)
		m_secMode = UA_MESSAGESECURITYMODE_INVALID;
	else if (secMode.compare("None") == 0)
		m_secMode = UA_MESSAGESECURITYMODE_NONE;
	else if (secMode.compare("Sign") == 0)
		m_secMode = UA_MESSAGESECURITYMODE_SIGN;
	else if (secMode.compare("SignAndEncrypt") == 0)
		m_secMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
	else 
	{
		m_secMode = UA_MESSAGESECURITYMODE_INVALID;
		Logger::getLogger()->error("Invalid Security mode '%s'", secMode.c_str());
	}
}

/**
 * Set the security policy
 *
 * @param value    Security policy string
 */
void
OPCUA::setSecPolicy(const std::string& secPolicy)
{
	if (!secPolicy.compare("Any"))
		m_secPolicy = "";
	else if (!secPolicy.compare("Basic256"))
		m_secPolicy = "http://opcfoundation.org/UA/SecurityPolicy#Basic256";
	else if (!secPolicy.compare("Basic256Sha256"))
		m_secPolicy = "http://opcfoundation.org/UA/SecurityPolicy#Basic256Sha25";
	else
	{
		m_secPolicy = "";
		Logger::getLogger()->error("Invalid Security policy '%s'", secPolicy.c_str());
	}
}

/**
 * Set the configuration for the plugin
 *
 * @param config	The configuration category
 */
void
OPCUA::setConfiguration(ConfigCategory *config)
{
	if (config->itemExists("asset"))
	{
		setAssetName(config->getValue("asset"));
	}
	else
	{
		setAssetName("opcua");
	}

	if (config->itemExists("subscribeById"))
	{
		string byId = config->getValue("subscribeById");
		if (byId.compare("true") == 0)
		{
			subscribeById(true);
		}
		else
		{
			subscribeById(false);
		}
	}

	// Now add the subscription data
	if (config->itemExists("subscription"))
	{
		string map = config->getValue("subscription");
		rapidjson::Document doc;
		doc.Parse(map.c_str());
		if (!doc.HasParseError())
		{
			if (doc.HasMember("subscriptions") && doc["subscriptions"].IsArray())
			{
				const rapidjson::Value& subs = doc["subscriptions"];
				for (rapidjson::SizeType i = 0; i < subs.Size(); i++)
				{
					addSubscription(subs[i].GetString());
				}
			}
			else
			{
				Logger::getLogger()->fatal("UPC UA plugin is missing a subscriptions array");
				throw exception();
			}
		}
	}


	if (config->itemExists("securityMode"))
	{
		setSecMode(config->getValue("securityMode"));
	}

	if (config->itemExists("securityPolicy"))
	{
		std::string secPolicy = config->getValue("securityPolicy");
		setSecPolicy(secPolicy);
	}

	if (config->itemExists("userAuthPolicy"))
	{
		setAuthPolicy(config->getValue("userAuthPolicy"));
	}

	if (config->itemExists("username"))
	{
		setUsername(config->getValue("username"));
	}

	if (config->itemExists("password"))
	{
		setPassword(config->getValue("password"));
	}
#if CERTIFICATES
	if (config->itemExists("caCert"))
	{
		setCaCert(config->getValue("caCert"));
	}

	if (config->itemExists("serverCert"))
	{
		setServerCert(config->getValue("serverCert"));
	}

	if (config->itemExists("clientCert"))
	{
		setClientCert(config->getValue("clientCert"));
	}

	if (config->itemExists("clientKey"))
	{
		setClientKey(config->getValue("clientKey"));
	}

	if (config->itemExists("caCrl"))
	{
		setRevocationList(config->getValue("caCrl"));
	}
#endif
}

/**
 * Data chenaged callback
 */
void OPCUA::dataChanged(const string *name, UA_DataValue *value)
{
	Logger::getLogger()->debug("Value changed for %s", name->c_str());
	DatapointValue dpv(0L);
	UA_Variant *variant = &(value->value);
	if (UA_Variant_isScalar(variant))
	{
		if  (variant->type == &UA_TYPES[UA_TYPES_BOOLEAN])
		{
			dpv = DatapointValue((long)*(UA_Boolean *)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_INT64])
		{
            		dpv = DatapointValue((long)*(UA_Int64*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_INT32])
		{
            		dpv = DatapointValue((long)*(UA_Int32*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_INT16])
		{
            		dpv = DatapointValue((long)*(UA_Int16*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_UINT64])
		{
            		dpv = DatapointValue((long)*(UA_UInt64*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_UINT32])
		{
            		dpv = DatapointValue((long)*(UA_UInt32*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_UINT16])
		{
            		dpv = DatapointValue((long)*(UA_UInt16*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_FLOAT])
		{
            		dpv = DatapointValue((double)*(UA_Float*)variant->data);
		}
		else if (variant->type == &UA_TYPES[UA_TYPES_DOUBLE])
		{
            		dpv = DatapointValue((double)*(UA_Double*)variant->data);
		}
	}
	vector<Datapoint *> points;
	points.push_back(new Datapoint(*name, dpv));
	Reading reading(*name, points);
	m_ingest(m_data, reading);
}
