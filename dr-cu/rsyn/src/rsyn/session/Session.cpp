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
 
#include <thread>
#include <iosfwd>
#include <mutex>
#include <boost/filesystem.hpp>

#include "Session.h"

#include "rsyn/util/Json.h"
#include "rsyn/phy/PhysicalService.h"
#include "rsyn/util/Environment.h"

namespace Rsyn {

// -----------------------------------------------------------------------------

void Session::init() {
    if (data) {
        delete data; 
    }

	std::setlocale(LC_ALL, "en_US.UTF-8");

	data = new SessionData();

	// TODO: hard coded
	data->clsInstallationPath = "../../rsyn/install";

	// Register services
	registerServices();

	// Register readers.
	registerReaders();
	
	// Create design.
	data->clsDesign.create("__Root_Design__");

	// Create library
	data->clsLibrary = Library(new LibraryData);
	data->clsLibrary->designData = data->clsDesign.data;

} // end constructor

// -----------------------------------------------------------------------------

Rsyn::Design Session::getDesign() {
	return data->clsDesign;
} // end method

// -----------------------------------------------------------------------------

Rsyn::Library Session::getLibrary() {
	return data->clsLibrary;
} // end method

// -----------------------------------------------------------------------------

Rsyn::Module Session::getTopModule() {
	return getDesign().getTopModule();
} // end method

// -----------------------------------------------------------------------------

Rsyn::PhysicalDesign Session::getPhysicalDesign() {
	if (isServiceRunning("rsyn.physical")) {
		PhysicalService *service = getService("rsyn.physical");
		return service->getPhysicalDesign();
	} else {
		return nullptr;
	} // end else
} // end method

// -----------------------------------------------------------------------------

std::string Session::mergePathAndFileName(const std::string &path, const std::string &fileName) {
	const char separator = boost::filesystem::path::preferred_separator;
	if (!path.empty() &&  (path.back() != separator && path.back() != '/')) {
		return path + separator + fileName;
	} else {
		return path + fileName;
	} // end else
} // end method

// -----------------------------------------------------------------------------

std::string Session::findFile(const std::string fileName, const std::string extraPath) {
	// Check if the file exists without any path.
	if (boost::filesystem::exists(fileName)) {
		return fileName;
	} // end if

	// Check if the file exists in the extra path.
	if (!extraPath.empty()) {
		const std::string fullFileName = mergePathAndFileName(extraPath, fileName);
		if (boost::filesystem::exists(fullFileName)) {
			return fullFileName;
		} // end if
	} // end if

	// Check if the file exists in the paths.
	for (const std::string &path : data->clsPaths) {
		const std::string fullFileName = mergePathAndFileName(path, fileName);
		if (boost::filesystem::exists(fullFileName)) {
			return fullFileName;
		} // end if
	} // end for

	// File not found.
	return "";
} // end method

// -----------------------------------------------------------------------------

void
Session::runReader(const std::string &name, const Rsyn::Json &params) {
	auto it = data->clsReaders.find(name);
	if (it == data->clsReaders.end()) {
		std::cout << "ERROR: Reader '" << name << "' was not "
				"registered.\n";
	} else {
		std::unique_ptr<Reader> reader(it->second());
		reader->load(this, params);
	} // end else
} // end method

// -----------------------------------------------------------------------------

bool
Session::startService(const std::string &name, const Rsyn::Json &params, const bool dontErrorOut) {
	auto it = data->clsServiceInstanciationFunctions.find(name);
	if (it == data->clsServiceInstanciationFunctions.end()) {
		if (!dontErrorOut) {
			std::cout << "ERROR: Service '" << name << "' was not "
					"registered.\n";
			std::exit(1);
		} // end if
		return false;
	} else {
		Service * service = getServiceInternal(name);
		if (!service) {
			service = it->second();
			service->start(*this, params);
			data->clsRunningServices[name] = service;

			return true;
		} else {
			if (!dontErrorOut) {
				std::cout << "WARNING: Service '" << name << "' is already running.\n";
			} // end if
			return false;
		} // end else
	} // end else
} // end method


} /* namespace Rsyn */
