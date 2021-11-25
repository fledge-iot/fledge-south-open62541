#ifndef _OPCUA_H
#define _OPCUA_H
/*
 * Fledge south service plugin
 *
 * Copyright (c) 2021 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <config_category.h>
#include <open62541/client_config_default.h>
#include <open62541/client_highlevel.h>
#include <open62541/client_subscriptions.h>
#include <open62541/plugin/securitypolicy.h>
#include <open62541/plugin/log.h>
#include <string>
#include <reading.h>
#include <logger.h>
#include <mutex>
#include <stdlib.h>
#include <map>
#include <thread>

class OPCUA
{
	public:
		OPCUA(const std::string& url);
		~OPCUA();
		void		clearSubscription();
		void		addSubscription(const std::string& parent);
		void		setAssetName(const std::string& name);
		void		restart();
		void		newURL(const std::string& url) { m_url = url; };
		void		subscribeById(bool byId) { m_subscribeById = byId; };
		void		start();
		void		stop();
		void		ingest(std::vector<Datapoint *>  points);
		void		registerIngest(void *data, void (*cb)(void *, Reading))
				{
					m_ingest = cb;
					m_data = data;
				}

        	void		setSecMode(const std::string& secMode);
	        void		setSecPolicy(const std::string& secPolicy);
		void		setAuthPolicy(const std::string& authPolicy) { m_authPolicy = authPolicy; }
		void		setUsername(const std::string& username) { m_username = username; }
		void		setPassword(const std::string& password) { m_password = password; }
		void		setCaCert(const std::string& cert) { m_certAuth = cert; }
		void		setServerCert(const std::string& cert) { m_serverPublic = cert; }
		void		setClientCert(const std::string& cert) { m_clientPublic = cert; }
		void		setClientKey(const std::string& key) { m_clientPrivate = key; }
		void		setRevocationList(const std::string& cert) { m_caCrl = cert; }
		void		setConfiguration(ConfigCategory *config);
		void		dataChanged(const std::string *name, UA_DataValue *value);
		void		threadStart();
	private:
		int				addSubscribe(const UA_NodeId *node, bool active);
		std::vector<std::string>	m_subscriptions;
		std::string			m_url;
		std::string			m_asset;
		UA_Client			*m_client;
		void				(*m_ingest)(void *, Reading);
		void				*m_data;
		std::mutex			m_configMutex;
		bool				m_subscribeById;
		bool				m_connected;

        
		std::string         		m_secPolicy;
		UA_MessageSecurityMode		m_secMode;
		std::string			m_authPolicy;
		std::string			m_username;
		std::string			m_password;
        
		std::string			m_serverPublic;
		std::string			m_clientPublic;
		std::string			m_clientPrivate;
		std::string			m_certAuth;
		std::string			m_caCrl;
		UA_Logger			m_UAlogger;
		std::map<std::string, bool>	m_subscriptionVariables;
		UA_UInt32			m_subscriptionId;
		std::thread			*m_thread;
		bool				m_threadStop;
};

#if 0
class OpcUaClient : public OpcUa::SubscriptionHandler
{ 
	public:
	  	OpcUaClient(OPCUA *opcua) : m_opcua(opcua) {};
		void DataChange(uint32_t handle,
				const OpcUa::Node & node,
				const OpcUa::Variant & val,
				OpcUa::AttributeId attr) override
		{
			// We don't support non-scalar or Nul values as conversion
			// to string does not work.
			if (!val.IsScalar() || val.IsNul())
			{
				return;
			}

			DatapointValue value(0L);
			switch (val.Type())
			{
				case OpcUa::VariantType::BYTE:
				case OpcUa::VariantType::SBYTE:
				{
					std::string sValue = val.ToString();
					std::string bValue;
					const char* replaceByte = "\\u%04d";
					for (size_t i = 0; i < sValue.length(); i++)
					{
						// Replace not printable char
						if (!isprint(sValue[i]))
						{
							char replace[strlen(replaceByte) + 1];
							snprintf(replace,
								 strlen(replaceByte) + 1,
								 replaceByte,
								 sValue[i]);
							bValue += replace;
						}
						else
						{
							bValue += sValue[i];
						}
					}
					value = DatapointValue(bValue);
					break;
				}
				case OpcUa::VariantType::INT16:
				{
					long lval = static_cast<int16_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::UINT16:
				{
					long lval = static_cast<uint16_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::INT32:
				{
					long lval = static_cast<int32_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::UINT32:
				{
					long lval = static_cast<uint32_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::INT64:
				{
					long lval = static_cast<int64_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::UINT64:
				{
					long lval = static_cast<uint64_t>(val);
					value = DatapointValue(lval);
					break;
				}
				case OpcUa::VariantType::FLOAT:
				{
					double fval = static_cast<float>(val);
					value = DatapointValue(fval);
					break;
				}
				case OpcUa::VariantType::DOUBLE:
				{
					double fval = static_cast<double>(val);
					value = DatapointValue(fval);
					break;
				}
				default:
				{
					std::string sValue = val.ToString();
					value = DatapointValue(sValue);
					break;
				}
			}

			std::vector<Datapoint *> points;
			std::string dpname = "Unknown";;
			try {
				OpcUa::NodeId id = node.GetId();
				if (id.IsInteger())
				{
					char buf[80];
					snprintf(buf, sizeof(buf), "%d", id.GetIntegerIdentifier());
					dpname = buf;
				}
				else
				{
					dpname = node.GetId().GetStringIdentifier();
				}
			} catch (std::exception& e) {
				Logger::getLogger()->error("No name for data change event: %s", e.what());
			}
			points.push_back(new Datapoint(dpname, value));
			m_opcua->ingest(points);
		};
	private:
		OPCUA		*m_opcua;
};
#endif
#endif
