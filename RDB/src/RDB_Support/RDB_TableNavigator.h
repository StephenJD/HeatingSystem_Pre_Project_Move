#pragma once
#include "RDB_B.h"
#include "RDB_AnswerID.h"
#include "RDB_ChunkHeader.h"
#include "RDB_Table.h"
#include "RDB_Answer_Locator.h"
#include "RDB_Capacities.h"
#include <Sum_Operators.h>
#include <Logging.h>
#include <Type_Traits.h>

#ifdef ZPSIM
	#include <ostream>
	#include <iomanip>
#endif


namespace RelationalDatabase {
	class RDB_B;
	class Answer_Locator;
#ifdef ZPSIM
	inline std::ostream & operator << (std::ostream & stream, const TB_Status & status) {
		return stream << F(" Status: ") << std::dec << 
			(status == TB_OK ? F("OK") : 
			(status == TB_RECORD_UNUSED ? F("unused") : 
			(status == TB_END_STOP ? F("end_stop") : 
		    (status == TB_BEFORE_BEGIN ? F("before_begin") :
			(status == TB_INVALID_TABLE ? F("invalid_table") : F("TB_something else"))))));
	}
#endif

	using namespace Rel_Ops;

	/// <summary>
/// Template class holding a copy record
/// Provides interface accessing fields
/// </summary>
	template <typename Record_T>
	class Answer_R : public Answer_Locator {
	public:
		Answer_R(const RecordSelector & rs) : Answer_Locator(rs) {}
		Answer_R(const TableNavigator & tableNaviagator) : Answer_Locator(tableNaviagator) {}
		Answer_R(const Answer_Locator & answerLocator) : Answer_Locator(answerLocator) {}
		Answer_R() = default;

		Answer_R & operator =(const RecordSelector_T<Record_T> & rs) {
			Answer_Locator::operator=(rs);
			return *this;
		}

		Answer_R & operator =(const RecordSelector & rs) {
			Answer_Locator::operator=(rs);
			return *this;
		}

		Answer_R & operator =(const Answer_Locator & answerLocator) {
			Answer_Locator::operator=(answerLocator);
			return *this;
		}

		Answer_R & operator =(const TableNavigator & tableNaviagator) {
			Answer_Locator::operator=(tableNaviagator);
			return *this;
		}

		// Queries
		// Modifiers
		RecordID field(int fieldIndex) { return rec().field(fieldIndex); }
		Answer_R * operator->() { return this; }

		TB_Status status() {
			return Answer_Locator::status(&_rec);
		};

		const Record_T & rec() const { return const_cast<Answer_R *>(this)->rec(); }

		Record_T & rec() {
			Answer_Locator::rec(&_rec);
			return _rec;
		}

		Record_T & get() {
			return _rec;
		}

		RecordID update();

	private:
		Record_T _rec;
	};

	/// <summary>
	/// Knows how to locate a record and how to move to the next/prev record.
	/// Manages Chunk-Chaining and Record insertion and Retrieval
	/// </summary>
	class TableNavigator {
	public:
		TableNavigator();
		TableNavigator(Table * t);
		TableNavigator(const Answer_Locator & answerLoc);
		TableNavigator(int id, TB_Status status);
		
		// Queries
		const AnswerID & answerID() const { return _currRecord; }
		bool operator==(const TableNavigator & other) const /*{ return _currRecord.id() == other._currRecord.id(); }*/;
		bool operator<(const TableNavigator & other) const /*{ return _currRecord.id() < other._currRecord.id(); }*/;
		TB_Status status() const /*{ return _currRecord.status(); }*/;
		int id() const /*{ return status() == TB_BEFORE_BEGIN ? -1 : _currRecord.id(); }*/;
		Table & table() const { return *_t; }
		
		// Modifiers
		template<typename Record_T>
		RecordID insert(const Record_T & newRecord);
		
		RecordID insertRecord(const void * record);
		//Table & table() { return *_t; }
		bool moveToFirstRecord();
		void moveToLastRecord();
		void moveToThisRecord(int recordID);
		void moveToNextUsedRecord(int direction);
		TableNavigator & operator++() { return (*this) += 1; }
		TableNavigator & operator--() { return (*this) += -1; }
		TableNavigator & operator+=(int offset);
		TableNavigator & operator-=(int offset) { return (*this) += -offset; }
		void setStatus(TB_Status status) /*{ _currRecord.setStatus(status); }*/;
		void setID(int id) /*{ _currRecord.setID(id); }*/;
		void setCurrent(AnswerID current) { _currRecord = current; }
		void next(RecordSelector & recSel, int moveBy);

	//protected:
		// Modifiers
		RecordID sortedInsert(void * recToInsert
			, bool(*compareRecords)(TableNavigator * left, const void * right, bool lhs_IsGreater)
			, void(*swapRecords)(TableNavigator *original, void * recToInsert));

		// Queries
		DB_Size_t firstRecordInChunk() const /*{ return _chunkAddr + Table::HeaderSize + noOfvalidRecordBytes(); }*/;
		static int validRecordByteNo(int chunkCapacity) { return chunkCapacity / RDB_B::ValidRecord_t_Capacity; }
		int noOfvalidRecordBytes() const { return validRecordByteNo(_chunk_header.chunkSize() - 1); }
		bool tableInvalid() /*{ return _t ? _t->dbInvalid() : true; }*/;
		RDB_B & db() const /*{return _t->db();}*/;
		Record_Size_t recordSize() const /*{ return _t->_rec_size; }*/;
		NoOf_Recs_t chunkCapacity() const /*{ return _t->maxRecordsInChunk(); }*/;
		NoOf_Recs_t endStopID() const /*{ return _t->maxRecordsInTable(); }*/;
		NoOf_Recs_t thisVRcapacity() const;
		bool isStartOfATable() const { return _chunk_header.isFirstChunk(); }
		bool chunkIsExtended() const { return !_chunk_header.isFinalChunk(); }
		bool recordIsUsed(ValidRecord_t usedRecords, int vrIndex) const;
		DB_Size_t recordAddress() const;
		bool haveMovedToUsedRecord(ValidRecord_t usedRecords, uint8_t vrIndex, int direction);
		bool lastUsedRecord(ValidRecord_t usedRecords, RecordID & usedRecID, RecordID & vr_start) const;
		bool haveReservedUnusedRecord(ValidRecord_t & usedRecords, RecordID & unusedRecID) const;
		TB_Size_t getAvailabilityByteAddress() const;
		TB_Size_t getAvailabilityBytesForThisRecord(ValidRecord_t & usedRecords, uint8_t & vrIndex) const;
		ValidRecord_t getFirstValidRecordByte() const;
		uint8_t getValidRecordIndex() const;

		//Modifiers
		bool haveMovedToNextChunck();
		bool isDeletedRecord();
		bool getLastUsedRecordInThisChunk(RecordID & usedRecID);
		void checkStatus();
		bool moveToUsedRecordInThisChunk(int direction);
		bool reserveFirstUnusedRecordInThisChunk();
		RecordID reserveUnusedRecordID();
		void saveHeader();
		void loadHeader();
		void extendChunkTo(TableID nextChunk);
		void shuffleRecordsBack(RecordID start, RecordID end);
		void shuffleValidRecordsByte(TB_Size_t availabilityByteAddr, bool shiftIn_UsedRecord);

	private:
		friend class RDB_B;
		friend class Answer_Locator;
		friend class Table;

		mutable unsigned long _timeValidRecordLastRead = 0;
		Table * _t = 0;
		ChunkHeader _chunk_header;
		AnswerID _currRecord;
		TableID _chunkAddr = 0;
		RecordID _chunk_start_recordID = 0;
		mutable uint8_t _VR_Byte_No = 0;
	};

	template <typename Record_T>
	RecordID TableNavigator::insert(const Record_T & newRecord) {
		static_assert(!typetraits::is_pointer<Record_T>::value, "The argument to insert must not be a pointer.");
		if (isSorted(table().insertionStrategy())) {
			Record_T recordToInsert = newRecord;
			auto compareRecords = [](TableNavigator * left, const void * right, bool lhsIsGreater)->bool {
				auto lhs = Answer_R<Record_T>{ *left }.rec();
				auto rhs = *reinterpret_cast<const Record_T*>(right);
				return lhsIsGreater ? rhs < lhs : lhs < rhs;
			};
			auto swapRecords = [](TableNavigator *original, void * recToInsert) {
				auto debug = *reinterpret_cast<Record_T*>(recToInsert);
				Answer_R<Record_T> currRec = *original;
				auto temp = currRec.rec();
				currRec.Answer_Locator::update(recToInsert);
				//*reinterpret_cast<Record_T*>(recToInsert) = currRec.get();
				*reinterpret_cast<Record_T*>(recToInsert) = temp;
			};

			auto insertPos = sortedInsert(&recordToInsert, compareRecords, swapRecords);

			auto unusedRec = insertRecord(&recordToInsert);

			if (unusedRec < insertPos) --insertPos;
			moveToThisRecord(insertPos);
			return insertPos;
		}
		else {
			return insertRecord(&newRecord);
		}
	}

	template <typename Record_T>
	RecordID Answer_R<Record_T>::update() {
		if (isSorted(_tb->insertionStrategy())) {
			deleteRecord();
			auto tableNav = TableNavigator(_tb);
			auto insertPos = tableNav.insert(get());
			*this = tableNav;
		}
		else {
			Answer_Locator::update(&get());
		}
		return id();
	}
}
