    /* Copyright 2014-2018 Rsyn
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#ifndef RSYN_SESSION_H
#define RSYN_SESSION_H

#include <string>

#include "rsyn/session/Service.h"
#include "rsyn/session/Reader.h"

#include "rsyn/util/Json.h"

#include "rsyn/core/Rsyn.h"
#include "rsyn/util/Units.h"

namespace Rsyn {

class PhysicalDesign;

typedef std::function<Service *()> ServiceInstantiatonFunction;
typedef std::function<Reader *()> ReaderInstantiatonFunction;

////////////////////////////////////////////////////////////////////////////////
// Session Data
////////////////////////////////////////////////////////////////////////////////

struct SessionData {
	////////////////////////////////////////////////////////////////////////////
	// Session Variables
	////////////////////////////////////////////////////////////////////////////
	
	std::map<std::string, Rsyn::Json> clsSessionVariables;

	////////////////////////////////////////////////////////////////////////////
	// Services
	////////////////////////////////////////////////////////////////////////////
	
	// Generic functions used to instantiate optimizers by name.
	std::unordered_map<std::string, ServiceInstantiatonFunction> clsServiceInstanciationFunctions;
	std::unordered_map<std::string, Service *> clsRunningServices;

	////////////////////////////////////////////////////////////////////////////
	// Loader
	////////////////////////////////////////////////////////////////////////////
	
	// Generic functions used to instantiate optimizers by name.
	std::unordered_map<std::string, ReaderInstantiatonFunction> clsReaders;
	
	////////////////////////////////////////////////////////////////////////////
	// Design/Library
	////////////////////////////////////////////////////////////////////////////

	Rsyn::Design clsDesign;
	Rsyn::Library clsLibrary;

	////////////////////////////////////////////////////////////////////////////
	// Configuration
	////////////////////////////////////////////////////////////////////////////
	std::string clsInstallationPath;
	
	////////////////////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////////////////////
	std::list<std::string> clsPaths;
}; // end struct

////////////////////////////////////////////////////////////////////////////////
// Session Proxy
////////////////////////////////////////////////////////////////////////////////

class Session : public Rsyn::Proxy<SessionData> {
public:
	Session() {
		data = nullptr;
	};
	
	Session &operator=(const Session &other) {
		data = other.data;
		return *this;
	}	
	
	void init();

private:

	////////////////////////////////////////////////////////////////////////////
	// Session Variables
	////////////////////////////////////////////////////////////////////////////

public:

	void setSessionVariable(const std::string &name, const Rsyn::Json &value) {
		data->clsSessionVariables[name] = value;
	} // end method

	void unsetSessionVariable(const std::string &name) {
		data->clsSessionVariables.erase(name);
	} // end method

	const bool getSessionVariableAsBool(const std::string &name, const bool defaultValue = false) {
		auto it = data->clsSessionVariables.find(name);
		return (it != data->clsSessionVariables.end())? it->second.get<bool>() : defaultValue;
	} // end method

	const int getSessionVariableAsInteger(const std::string &name, const int defaultValue = 0) {
		auto it = data->clsSessionVariables.find(name);
		return (it != data->clsSessionVariables.end())? it->second.get<int>() : defaultValue;
	} // end method

	const float getSessionVariableAsFloat(const std::string &name, const float defaultValue = 0.0f) {
		auto it = data->clsSessionVariables.find(name);
		return (it != data->clsSessionVariables.end())? it->second.get<float>() : defaultValue;
	} // end method

	const std::string getSessionVariableAsString(const std::string &name, const std::string &defaultValue = "") {
		auto it = data->clsSessionVariables.find(name);
		return (it != data->clsSessionVariables.end())? it->second.get<std::string>() : defaultValue;
	} // end method

	const Rsyn::Json getSessionVariableAsJson(const std::string &name, const Rsyn::Json &defaultValue = {}) {
		auto it = data->clsSessionVariables.find(name);
		return (it != data->clsSessionVariables.end())? it->second : defaultValue;
	} // end method
	
	////////////////////////////////////////////////////////////////////////////
	// Services
	////////////////////////////////////////////////////////////////////////////

	// Register services.
	void registerServices();

public:
	
	// Helper class to allow seamless casting from a Service pointer to any type
	// which implements operator=(Service *) is is directly compatible.
	
	class ServiceHandler {
	private:
		Service *clsService;
		
	public:
		ServiceHandler(Service *service) : clsService(service) {}
		
		template<typename T>
		operator T *() {
			T * pointer = dynamic_cast<T *>(clsService);
			if (pointer != clsService) {
				std::cout << "ERROR: Trying to cast a service to the wrong type.\n";
				throw Exception("Trying to cast a service to the wrong type.");
			} // end if
			return pointer;
		} // end operator
	}; // end class
		
	// Register a service.
	template<typename T>
	void registerService(const std::string &name) {
		auto it = data->clsServiceInstanciationFunctions.find(name);
		if (it != data->clsServiceInstanciationFunctions.end()) {
			std::cout << "ERROR: Service '" << name << "' was already "
					"registered.\n";
			std::exit(1);
		} else {
			data->clsServiceInstanciationFunctions[name] = []() -> Service *{
				return new T();
			};
		} // end else
	} // end method
	
	// Start a service.
	bool startService(const std::string &name, const Rsyn::Json &params = {}, const bool dontErrorOut = false);
	
	// Gets a running service.
	ServiceHandler getService(const std::string &name,
			const ServiceRequestType requestType = SERVICE_MANDATORY) {
		Service *service = getServiceInternal(name);
		if (!service && (requestType == SERVICE_MANDATORY)) {
			std::cout << "ERROR: Service '" << name << "' was not started.\n";
			throw Exception("ERROR: Service '" + name + "' was not started");
		} // end if
		return ServiceHandler(service);
	} // end method

	// Checks if a service is registered.
	bool isServiceRegistered(const std::string &name) {
		auto it = data->clsServiceInstanciationFunctions.find(name);
		return  (it != data->clsServiceInstanciationFunctions.end());
	} // end method

	// Checks if a service is running.
	bool isServiceRunning(const std::string &name) {
		return getServiceInternal(name) != nullptr;
	} // end method
	
private:
	
	Service * getServiceInternal(const std::string &name) {
		auto it = data->clsRunningServices.find(name);
		return it == data->clsRunningServices.end()? nullptr : it->second;
	} // end method

	void listService(std::ostream & out = std::cout) {
		out<<"List of services ";
		out<<"([R] -> Running, [S] -> Stopped):\n";
		// print only running services
		for (std::pair<std::string, ServiceInstantiatonFunction> srv : data->clsServiceInstanciationFunctions) {
			if (!isServiceRunning(srv.first))
				continue;
			out << "\t[R] " << srv.first << "\n";
		} // end for 
		// print only stopped services 
		for (std::pair<std::string, ServiceInstantiatonFunction> srv : data->clsServiceInstanciationFunctions) {
			if (isServiceRunning(srv.first))
				continue;
			out << "\t[S] "<<srv.first << "\n";
		} // end for 
		out << "--------------------------------------\n";
	} /// end method 
	
	////////////////////////////////////////////////////////////////////////////
	// Reader
	////////////////////////////////////////////////////////////////////////////
private:
	void listReader(std::ostream & out = std::cout) {
		out<<"List of registered Readers:\n";
		for(std::pair<std::string, ReaderInstantiatonFunction> reader : data->clsReaders) {
			out<<"\t"<<reader.first<<"\n";
		} // end for 
		out<<"--------------------------------------\n";
	} /// end method

	// Register loader.
	void registerReaders();

public:
		
	// Register a loader.
	template<typename T>
	void registerReader(const std::string &name) {
		auto it = data->clsReaders.find(name);
		if (it != data->clsReaders.end()) {
			std::cout << "ERROR: Reader '" << name << "' was already "
					"registered.\n";
			std::exit(1);
		} else {
			data->clsReaders[name] = []() -> Reader *{
				return new T();
			};
		} // end else
	} // end method
	
	// Run an loader.
	void runReader(const std::string &name, const Rsyn::Json &params = {});
	
	////////////////////////////////////////////////////////////////////////////
	// Misc
	////////////////////////////////////////////////////////////////////////////

	Rsyn::Design getDesign();
	Rsyn::Library getLibrary();
	Rsyn::Module getTopModule();
	Rsyn::PhysicalDesign getPhysicalDesign();

	const std::string &getInstallationPath() { return data->clsInstallationPath; }

	////////////////////////////////////////////////////////////////////////////
	// Utilities
	////////////////////////////////////////////////////////////////////////////

private:

	std::string mergePathAndFileName(const std::string &path, const std::string &fileName);

public:

	//! @brief Find a file in the current path. If found, returns its absolute
	//!        path, an empty string otherwise.
	//! @param extraPath can be used to specify an extra path location besides
	//!        the one stored internally in the current path list.
	std::string findFile(const std::string fileName, const std::string extraPath = "");
	
}; // end class

} // end namespace

#endif /* INFRA_ICCAD15_SESSION_H_ */
