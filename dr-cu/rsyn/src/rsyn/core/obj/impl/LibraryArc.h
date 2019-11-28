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
 
namespace Rsyn {

inline
Design 
LibraryArc::getDesign() {
	return data->design;
} // end method

// -----------------------------------------------------------------------------

inline
const Design 
LibraryArc::getDesign() const {
	return data->design;
} // end method

// -----------------------------------------------------------------------------

inline
std::string
LibraryArc::getName() const {
	return getFromName() + "->" + getToName();		
} // end method

// -----------------------------------------------------------------------------

inline
std::string
LibraryArc::getFullName() const {
	return getLibraryCell().getName() + ":" + getName();
} // end method

// -----------------------------------------------------------------------------

inline
std::string
LibraryArc::getFromName() const {
	return getFromLibraryPin().getName();
} // end method

// -----------------------------------------------------------------------------

inline
std::string
LibraryArc::getToName() const {
	return getToLibraryPin().getName();
} // end method

// -----------------------------------------------------------------------------

inline
LibraryCell
LibraryArc::getLibraryCell() const {
	return data->lcell;
} // end method

// -----------------------------------------------------------------------------

inline
LibraryPin
LibraryArc::getFromLibraryPin() const {
	return data->from;
} // end method

// -----------------------------------------------------------------------------

inline
LibraryPin
LibraryArc::getToLibraryPin() const {
	return data->to;
} // end method

// -----------------------------------------------------------------------------

inline
int
LibraryArc::getIndex() const {
	return data->index;
} // end method

} // end namespace