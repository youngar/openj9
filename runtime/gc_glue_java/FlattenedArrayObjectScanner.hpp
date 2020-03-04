/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(FLATTENEDARRAYOBJECTSCANNER_HPP_)
#define FLATTENEDARRAYOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "objectdescription.h"
#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "HeadlessMixedObjectScanner.hpp"
#include "IndexableObjectScanner.hpp"

class GC_FlattenedArrayObjectScanner : public GC_IndexableObjectScanner
{
	/* Data Members */
private:
	MM_EnvironmentBase *_env;
	fomrobject_t *_elementPtr; /**< pointer to the current element of the array */
	J9Class *_elementClazzPtr; /**< pointer to the array element class */
	GC_HeadlessMixedObjectScanner _objectScanner; /**< Used to scan individual elements */

protected:

public:

	/* Methods */
private:

protected:

	/**
	 * @param env The scanning thread environment
	 * @param[in] arrayPtr pointer to the array to be processed
	 * @param[in] basePtr pointer to the first contiguous array cell
	 * @param[in] limitPtr pointer to end of last contiguous array cell
	 * @param[in] scanPtr pointer to the array cell where scanning will start
	 * @param[in] endPtr pointer to the array cell where scanning will stop
	 * @param[in] scanMap The first scan map
	 * @param[in] elementSize The size of every element
	 * @param[in] elementClazzPtr Pointer to the element's class
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_FlattenedArrayObjectScanner(
		MM_EnvironmentBase *env
		, omrobjectptr_t arrayPtr
		, fomrobject_t *basePtr
		, fomrobject_t *limitPtr
		, fomrobject_t *scanPtr
		, fomrobject_t *endPtr
		, uintptr_t scanMap
		, uintptr_t elementSize
		, J9Class *elementClazzPtr
		, uintptr_t flags)
	: GC_IndexableObjectScanner(env, arrayPtr, basePtr, limitPtr, scanPtr, endPtr, scanMap, elementSize, flags)
	, _env(env)
	, _elementPtr(_scanPtr)
	, _elementClazzPtr(elementClazzPtr)
	, _objectScanner(env, elementClazzPtr, scanPtr, flags)
	{
		_typeId = __FUNCTION__;
	}

	MMINLINE void
	initialize(MM_EnvironmentBase *env, J9Class *elementClazzPtr)
	{
		GC_IndexableObjectScanner::initialize(env);
		_objectScanner.initialize(env, elementClazzPtr);
	}

public:

	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr pointer to the array to be processed
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param[in] flags Scanning context flags
	 * @param[in] splitAmount If >0, the number of elements to include for this scanner instance; if 0, include all elements
	 * @param[in] startIndex The index of the first element to scan
	 */
	MMINLINE static GC_FlattenedArrayObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags, uintptr_t splitAmount, uintptr_t startIndex = 0)
	{
		GC_FlattenedArrayObjectScanner *objectScanner = (GC_FlattenedArrayObjectScanner *)allocSpace;
		GC_ArrayObjectModel *arrayObjectModel = &(env->getExtensions()->indexableObjectModel);
		J9Class *clazzPtr = J9GC_J9OBJECT_CLAZZ(objectPtr, env);
		J9ArrayClass *j9ArrayClass = (J9ArrayClass *) clazzPtr;

		// TODO are these always the same? 
		Assert_MM_true(j9ArrayClass->componentType == j9ArrayClass->leafComponentType);

		J9Class *elementClass = j9ArrayClass->componentType;

		omrarrayptr_t arrayPtr = (omrarrayptr_t)objectPtr;
		uintptr_t sizeInElements = arrayObjectModel->getSizeInElements(arrayPtr);
		uintptr_t elementSize = J9ARRAYCLASS_GET_STRIDE(clazzPtr);
		env->getExtensions()->mixedObjectModel.getSizeInBytesWithoutHeader(elementClass);
		fomrobject_t *basePtr = (fomrobject_t *)arrayObjectModel->getDataPointerForContiguous(arrayPtr);
		fomrobject_t *limitPtr = basePtr + (sizeInElements * elementSize);
		fomrobject_t *scanPtr = basePtr + (startIndex * elementSize);
		fomrobject_t *endPtr = limitPtr;
		if (!GC_ObjectScanner::isIndexableObjectNoSplit(flags) && (splitAmount != 0)) {
			endPtr = scanPtr + (splitAmount * elementSize);
			if (endPtr > limitPtr) {
				endPtr = limitPtr;
			}
		}

		new(objectScanner) GC_FlattenedArrayObjectScanner(env, objectPtr, basePtr, limitPtr, scanPtr, endPtr, 0, elementSize, elementClass, flags);
		objectScanner->initialize(env, elementClass);
		if (0 != startIndex) {
			objectScanner->clearHeadObjectScanner();
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

	/**
	 * @param env The scanning thread environment
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param splitAmount The maximum number of array elements to include
	 * @return Pointer to split scanner in allocSpace
	 */
	virtual GC_IndexableObjectScanner *
	splitTo(MM_EnvironmentBase *env, void *allocSpace, uintptr_t splitAmount)
	{
		Assert_MM_unimplemented();
	}

	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t *slotMap, bool *hasNextSlotMap)
	{
		fomrobject_t *result = _objectScanner.getNextSlotMap(slotMap, hasNextSlotMap);
		if (result) {
			*hasNextSlotMap = true;
			return result;
		}

		// No more to scan in the current element, so advance to the next element
		_elementPtr += _elementSize;
		if (_elementPtr < _endPtr) {
			new (&_objectScanner) GC_HeadlessMixedObjectScanner(_env, _elementClazzPtr, _elementPtr, _flags);
			_objectScanner.initialize(_env, _elementClazzPtr);
			result =  _objectScanner.getNextSlotMap(slotMap, hasNextSlotMap);
		} else {
			// No more elements in the array
			*slotMap = 0;
			*hasNextSlotMap = false;
			result = NULL;
		}
		return result;
	}

#if defined(OMR_GC_LEAF_BITS)
	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] leafMap the leaf bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t *slotMap, uintptr_t *leafMap, bool *hasNextSlotMap)
	{
		fomrobject_t *result = _objectScanner.getNextSlotMap(slotMap, leafMap, hasNextSlotMap);
		if (result) {
			*hasNextSlotMap = true;
			return result;
		}

		// No more to scan in the current element, so advance to the next element
		_elementPtr += _elementSize;
		if (_elementPtr < _endPtr) {
			new (&_objectScanner) GC_HeadlessMixedObjectScanner(_env, _elementClazzPtr, _elementPtr, _flags);
			_objectScanner.initialize(_env, _elementClazzPtr);
			result =  _objectScanner.getNextSlotMap(slotMap, leafMap, hasNextSlotMap);
		} else {
			// No more elements in the array
			*slotMap = 0;
			*hasNextSlotMap = false;
			result = NULL;
		}

		return result;
	}
#endif /* defined(OMR_GC_LEAF_BITS) */
};

#endif /* FLATTENEDARRAYOBJECTSCANNER_HPP_ */