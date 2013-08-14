//
//  CBAddressStorage.c
//  cbitcoin
//
//  Created by Matthew Mitchell on 18/01/2013.
//  Copyright (c) 2013 Matthew Mitchell
//
//  This file is part of cbitcoin. It is subject to the license terms
//  in the LICENSE file found in the top-level directory of this
//  distribution and at http://www.cbitcoin.com/license.html. No part of
//  cbitcoin, including this file, may be copied, modified, propagated,
//  or distributed except according to the terms contained in the
//  LICENSE file.

//  SEE HEADER FILE FOR DOCUMENTATION

#include "CBAddressStorage.h"

uint8_t CB_KEY_ARRAY[18];
uint8_t CB_DATA_ARRAY[20];

bool CBNewAddressStorage(CBDepObject * storage, CBDepObject database){
	CBAddressStorage * self = malloc(sizeof(*self));
	self->database = database.ptr;
	self->addrs = CBLoadIndex(self->database, CB_INDEX_ADDRS, 18, 10000);
	if (NOT self->addrs) {
		CBLogError("Could not load address index.");
		return false;
	}
	storage->ptr = self;
	return true;
}
void CBFreeAddressStorage(CBDepObject self){
	free(self.ptr);
}
bool CBAddressStorageDeleteAddress(CBDepObject uself, void * address){
	CBAddressStorage * self = uself.ptr;
	CBNetworkAddress * addrObj = address;
	memcpy(CB_KEY_ARRAY, CBByteArrayGetData(addrObj->ip), 16);
	CBInt16ToArray(CB_KEY_ARRAY, 16, addrObj->port);
	// Remove address
	if (NOT CBDatabaseRemoveValue(self->addrs, CB_KEY_ARRAY, false)) {
		CBLogError("Could not remove an address from storage.");
		return false;
	}
	// Decrease number of addresses
	CBInt32ToArray(self->database->current.extraData, CB_NUM_ADDRS, CBArrayToInt32(self->database->current.extraData, CB_NUM_ADDRS) - 1);
	// Commit changes
	if (NOT CBDatabaseStage(self->database)) {
		CBLogError("Could not commit the removal of a network address.");
		return false;
	}
	return true;
}
uint64_t CBAddressStorageGetNumberOfAddresses(CBDepObject uself){
	return CBArrayToInt64(((CBAddressStorage *)uself.ptr)->database->current.extraData, 0);
}
bool CBAddressStorageLoadAddresses(CBDepObject uself, void * addrMan){
	CBAddressStorage * self = uself.ptr;
	CBNetworkAddressManager * addrManObj = addrMan;
	CBDatabaseRangeIterator it = {(uint8_t []){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, (uint8_t []){0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}, self->addrs};
	CBIndexFindStatus status = CBDatabaseRangeIteratorFirst(&it);
	if (status == CB_DATABASE_INDEX_ERROR) {
		CBLogError("Could not get the first address from the database.");
		return false;
	}
	while(status == CB_DATABASE_INDEX_FOUND) {
		uint8_t * key = CBDatabaseRangeIteratorGetKey(&it);
		if (NOT CBDatabaseRangeIteratorRead(&it, CB_DATA_ARRAY, 20, 0)) {
			CBLogError("Could not read address data from database.");
			return false;
		}
		// Create network address object and add to the address manager.
		CBByteArray * ip = CBNewByteArrayWithDataCopy(key, 16);
		CBNetworkAddress * addr = CBNewNetworkAddress(CBArrayToInt64(CB_DATA_ARRAY, 0),
													  ip,
													  CBArrayToInt16(key, 16),
													  (CBVersionServices) CBArrayToInt64(CB_DATA_ARRAY, 8),
													  true);
		addr->penalty = CBArrayToInt32(CB_DATA_ARRAY, 16);
		CBNetworkAddressManagerTakeAddress(addrManObj, addr);
		CBReleaseObject(ip);
		status = CBDatabaseRangeIteratorNext(&it);
		if (status == CB_DATABASE_INDEX_ERROR) {
			CBLogError("Could not iterate to the next address in the database.");
			return false;
		}
	}
	return true;
}
bool CBAddressStorageSaveAddress(CBDepObject uself, void * address){
	CBAddressStorage * self = uself.ptr;
	CBNetworkAddress * addrObj = address;
	// Create key
	memcpy(CB_KEY_ARRAY, CBByteArrayGetData(addrObj->ip), 16);
	CBInt16ToArray(CB_KEY_ARRAY, 16, addrObj->port);
	// Create data
	CBInt64ToArray(CB_DATA_ARRAY, 0, addrObj->lastSeen);
	CBInt64ToArray(CB_DATA_ARRAY, 8, (uint64_t) addrObj->services);
	CBInt32ToArray(CB_DATA_ARRAY, 16, addrObj->penalty);
	// Write data
	CBDatabaseWriteValue(self->addrs, CB_KEY_ARRAY, CB_DATA_ARRAY, 20);
	// Increase the number of addresses
	CBInt32ToArray(self->database->current.extraData, CB_NUM_ADDRS, CBArrayToInt32(self->database->current.extraData, CB_NUM_ADDRS) + 1);
	// Commit changes
	if (NOT CBDatabaseStage(self->database)) {
		CBLogError("Could not commit adding a new network address to storage.");
		return false;
	}
	return true;
}
