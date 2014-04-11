/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "illusions/illusions.h"
#include "illusions/scriptresource.h"
#include "illusions/scriptman.h"

namespace Illusions {

// ScriptResourceLoader

void ScriptResourceLoader::load(Resource *resource) {
	debug(2, "ScriptResourceLoader::load() Loading script %08X from %s...", resource->_resId, resource->_filename.c_str());

	ScriptResource *scriptResource = new ScriptResource();
	scriptResource->load(resource);

	_vm->_scriptResource = scriptResource;
	
}

void ScriptResourceLoader::unload(Resource *resource) {
	delete _vm->_scriptResource;
}

void ScriptResourceLoader::buildFilename(Resource *resource) {
	resource->_filename = Common::String::format("%08X.scr", resource->_resId);
}

bool ScriptResourceLoader::isFlag(int flag) {
	return
		flag == kRlfLoadFile;
}

Properties::Properties()
	: _count(0), _properties(0) {
}

void Properties::init(uint count, byte *properties) {
	_count = count;
	_properties = properties;
}

void Properties::clear() {
	uint size = (_count >> 3) + 1;
	for (uint i = 0; i < size; ++i)
		_properties[i] = 0;
}

bool Properties::get(uint32 propertyId) {
	uint index;
	byte mask;
	getProperyPos(propertyId, index, mask);
	return (_properties[index] & mask) != 0;
}

void Properties::set(uint32 propertyId, bool value) {
	uint index;
	byte mask;
	getProperyPos(propertyId, index, mask);
	if (value)
		_properties[index] |= mask;
	else
		_properties[index] &= ~mask;
}

void Properties::getProperyPos(uint32 propertyId, uint &index, byte &mask) {
	propertyId &= 0xFFFF;
	index = propertyId >> 3;
	mask = 1 << (propertyId & 7);
}

// BlockCounters

BlockCounters::BlockCounters()
	: _count(0), _blockCounters(0) {
}

void BlockCounters::init(uint count, byte *blockCounters) {
	_count = count;
	_blockCounters = blockCounters;
}

void BlockCounters::clear() {
	for (uint i = 0; i < _count; ++i)
		_blockCounters[i] = 0;
}

byte BlockCounters::get(uint index) {
	return _blockCounters[index - 1] & 0x3F;
}

void BlockCounters::set(uint index, byte value) {
	_blockCounters[index - 1] ^= (_blockCounters[index - 1] ^ value) & 0x3F;
}

// TriggerCause

void TriggerCause::load(Common::SeekableReadStream &stream) {
	_verbId = stream.readUint32LE();
	_objectId2 = stream.readUint32LE();
	_codeOffs = stream.readUint32LE();
	
	debug(2, "TriggerCause::load() _verbId: %08X; _objectId2: %08X; _codeOffs: %08X",
		_verbId, _objectId2, _codeOffs);
}

// TriggerObject

TriggerObject::TriggerObject()
	: _causesCount(0), _causes(0) {
}

TriggerObject::~TriggerObject() {
	delete[] _causes;
}

void TriggerObject::load(byte *dataStart, Common::SeekableReadStream &stream) {
	_objectId = stream.readUint32LE();
	_causesCount = stream.readUint16LE();
	stream.skip(2); // Skip padding
	debug(2, "TriggerObject::load() _objectId: %08X; _causesCount: %d",
		_objectId, _causesCount);
	_causes = new TriggerCause[_causesCount];
	for (uint i = 0; i < _causesCount; ++i)
		_causes[i].load(stream);
}

bool TriggerObject::findTriggerCause(uint32 verbId, uint32 objectId2, uint32 &codeOffs) {
	for (uint i = 0; i < _causesCount; ++i)
		if (_causes[i]._verbId == verbId && _causes[i]._objectId2 == objectId2) {
			codeOffs = _causes[i]._codeOffs;
			return true;
		}
	return false;
}

void TriggerObject::fixupProgInfosDuckman() {
	for (uint i = 0; i < _causesCount; ++i)
		_causes[i]._verbId &= 0xFFFF;
}

// ProgInfo

ProgInfo::ProgInfo()
	: _triggerObjectsCount(0), _triggerObjects(0),
	_resourcesCount(0), _resources(0) {
}

ProgInfo::~ProgInfo() {
	delete[] _triggerObjects;
	delete[] _resources;
}

char *debugW2I(byte *wstr) {
	static char buf[65];
	char *p = buf;
	while (*wstr != 0) {
		*p++ = *wstr;
		wstr += 2;
	}
	*p = 0;
	return buf;
}

void ProgInfo::load(byte *dataStart, Common::SeekableReadStream &stream) {
	_id = stream.readUint16LE();
	_unk = stream.readUint16LE();
	_name = dataStart + stream.pos();
	stream.skip(128);
	_triggerObjectsCount = stream.readUint16LE();
	_resourcesCount = stream.readUint16LE();
	debug(2, "\nProgInfo::load() _id: %d; _unk: %d; _name: [%s]",
		_id, _unk, debugW2I(_name));
	uint32 triggerObjectsListOffs = stream.readUint32LE();
	if (_resourcesCount > 0) {
		_resources = new uint32[_resourcesCount];
		for (uint i = 0; i < _resourcesCount; ++i)
			_resources[i] = stream.readUint32LE();
	}
	if (_triggerObjectsCount > 0) {
		_triggerObjects = new TriggerObject[_triggerObjectsCount];
		for (uint i = 0; i < _triggerObjectsCount; ++i) {
			stream.seek(triggerObjectsListOffs + i * 4);
			uint32 triggerObjectOffs = stream.readUint32LE();
			stream.seek(triggerObjectOffs);
			_triggerObjects[i].load(dataStart, stream);
		}
	}
}

bool ProgInfo::findTriggerCause(uint32 verbId, uint32 objectId2, uint32 objectId, uint32 &codeOffs) {
	TriggerObject *triggerObject = findTriggerObject(objectId);
	if (triggerObject)
		return triggerObject->findTriggerCause(verbId, objectId2, codeOffs);
	return false;
}

void ProgInfo::getResources(uint &resourcesCount, uint32 *&resources) {
	resourcesCount = _resourcesCount;
	resources = _resources;
}

TriggerObject *ProgInfo::findTriggerObject(uint32 objectId) {
	for (uint i = 0; i < _triggerObjectsCount; ++i)
		if (_triggerObjects[i]._objectId == objectId)
			return &_triggerObjects[i];
	return 0;
}

void ProgInfo::fixupProgInfosDuckman() {
	for (uint i = 0; i < _triggerObjectsCount; ++i)
		_triggerObjects[i].fixupProgInfosDuckman();
}

// ScriptResource

ScriptResource::ScriptResource()
	: _codeOffsets(0), _objectMap(0) {
}

ScriptResource::~ScriptResource() {
	delete[] _codeOffsets;
	delete[] _objectMap;
}

void ScriptResource::load(Resource *resource) {
	_data = resource->_data;
	_dataSize = resource->_dataSize;

	Common::MemoryReadStream stream(_data, _dataSize, DisposeAfterUse::NO);
	
	uint32 objectMapOffs, progInfosOffs;
	_objectMapCount = 0;
	
	if (resource->_gameId == kGameIdBBDOU) {
		progInfosOffs = 0x18;
	} else if (resource->_gameId == kGameIdDuckman) {
		for (uint i = 0; i < 27; ++i)
			_soundIds[i] = stream.readUint32LE();
		progInfosOffs = 0x8C;
	}
	
	stream.skip(4); // Skip unused

	// Read item counts
	uint propertiesCount = stream.readUint16LE();
	uint blockCountersCount = stream.readUint16LE();
	if (resource->_gameId == kGameIdDuckman)
		_objectMapCount = stream.readUint16LE();
	_codeCount = stream.readUint16LE();
	_progInfosCount = stream.readUint16LE();
	if (resource->_gameId == kGameIdDuckman)
		stream.readUint16LE();//Unused?

	// Read item offsets
	uint32 propertiesOffs = stream.readUint32LE();
	uint32 blockCountersOffs = stream.readUint32LE();
	if (resource->_gameId == kGameIdDuckman)
		objectMapOffs = stream.readUint32LE();
	uint32 codeTblOffs = stream.readUint32LE();
	
	debug(2, "ScriptResource::load() propertiesCount: %d; blockCountersCount: %d; _codeCount: %d; _progInfosCount: %d; _objectMapCount: %d",
		propertiesCount, blockCountersCount, _codeCount, _progInfosCount, _objectMapCount);
	debug(2, "ScriptResource::load() propertiesOffs: %08X; blockCountersOffs: %08X; codeTblOffs: %08X; objectMapOffs: %08X",
		propertiesOffs, blockCountersOffs, codeTblOffs, objectMapOffs);
	// Init properties
	_properties.init(propertiesCount, _data + propertiesOffs);
	
	// Init blockcounters
	_blockCounters.init(blockCountersCount, _data + blockCountersOffs);
	
	_codeOffsets = new uint32[_codeCount];
	stream.seek(codeTblOffs);
	for (uint i = 0; i < _codeCount; ++i)
		_codeOffsets[i] = stream.readUint32LE();

	_progInfos = new ProgInfo[_progInfosCount];
	for (uint i = 0; i < _progInfosCount; ++i) {
		stream.seek(progInfosOffs + i * 4);
		uint32 progInfoOffs = stream.readUint32LE();
		stream.seek(progInfoOffs);
		_progInfos[i].load(_data, stream);
	}
	
	if (_objectMapCount > 0) {
		_objectMap = new uint32[_objectMapCount];
		stream.seek(objectMapOffs);
		for (uint i = 0; i < _objectMapCount; ++i) {
			_objectMap[i] = stream.readUint32LE();
			stream.skip(4);
		}
	}
	
	stream.seek(0x6C);
	_field6C = stream.readUint32LE();
	
	if (resource->_gameId == kGameIdDuckman)
		fixupProgInfosDuckman();

}

byte *ScriptResource::getThreadCode(uint32 threadId) {
	return _data + _codeOffsets[(threadId & 0xFFFF) - 1];
}

byte *ScriptResource::getCode(uint32 codeOffs) {
	return _data + codeOffs;
}

ProgInfo *ScriptResource::getProgInfo(uint32 index) {
	if (index > 0 && index <= _progInfosCount)
		return &_progInfos[index - 1];
	return 0;
}

uint32 ScriptResource::getObjectActorTypeId(uint32 objectId) {
	return _objectMap[(objectId & 0xFFFF) - 1];
}

void ScriptResource::fixupProgInfosDuckman() {
	for (uint i = 0; i < _progInfosCount; ++i)
		_progInfos[i].fixupProgInfosDuckman();
}

} // End of namespace Illusions