//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/execution/operator/csv_scanner/csv_error.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/typedefs.hpp"
#include "duckdb/common/types/hash.hpp"
#include "duckdb/common/mutex.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/common/types/string_type.hpp"
#include "duckdb/execution/operator/csv_scanner/csv_reader_options.hpp"

namespace duckdb {

//! Object that holds information on how many lines each csv batch read.

class LinesPerBoundary {
public:
	LinesPerBoundary();
	LinesPerBoundary(idx_t boundary_idx, idx_t lines_in_batch);

	idx_t boundary_idx = 0;
	idx_t lines_in_batch = 0;

	bool operator<(const LinesPerBoundary &other) const {
		if (boundary_idx < other.boundary_idx) {
			return true;
		}
		return lines_in_batch < other.lines_in_batch;
	}
};

enum CSVErrorType : uint8_t {
	CAST_ERROR = 0,                // If when casting a value from string to the column type fails
	COLUMN_NAME_TYPE_MISMATCH = 1, // If there is a mismatch between Column Names and Types
	INCORRECT_COLUMN_AMOUNT = 2,   // If the CSV is missing a column
	UNTERMINATED_QUOTES = 3,       // If a quote is not terminated
	SNIFFING = 4,          // If something went wrong during sniffing and was not possible to find suitable candidates
	MAXIMUM_LINE_SIZE = 5, // Maximum line size was exceeded by a line in the CSV File
	NULLPADDED_QUOTED_NEW_VALUE = 6, // If the null_padding option is set and we have quoted new values in parallel
	INVALID_UNICODE = 7

};

class CSVError {
public:
	CSVError() {};
	CSVError(string error_message, CSVErrorType type, idx_t column_idx, vector<Value> row, LinesPerBoundary error_info);
	CSVError(string error_message, CSVErrorType type, LinesPerBoundary error_info);
	//! Produces error messages for column name -> type mismatch.
	static CSVError ColumnTypesError(case_insensitive_map_t<idx_t> sql_types_per_column, const vector<string> &names);
	//! Produces error messages for casting errors
	static CSVError CastError(const CSVReaderOptions &options, string &column_name, string &cast_error,
	                          idx_t column_idx, vector<Value> &row, LinesPerBoundary error_info);
	//! Produces error for when the line size exceeds the maximum line size option
	static CSVError LineSizeError(const CSVReaderOptions &options, idx_t actual_size, LinesPerBoundary error_info);
	//! Produces error for when the sniffer couldn't find viable options
	static CSVError SniffingError(string &file_path);
	//! Produces error messages for unterminated quoted values
	static CSVError UnterminatedQuotesError(const CSVReaderOptions &options, string_t *vector_ptr,
	                                        idx_t vector_line_start, idx_t current_column, LinesPerBoundary error_info);
	//! Produces error messages for null_padding option is set and we have quoted new values in parallel
	static CSVError NullPaddingFail(const CSVReaderOptions &options, LinesPerBoundary error_info);
	//! Produces error for incorrect (e.g., smaller and lower than the predefined) number of columns in a CSV Line
	static CSVError IncorrectColumnAmountError(const CSVReaderOptions &options, string_t *vector_ptr,
	                                           idx_t vector_line_start, idx_t actual_columns,
	                                           LinesPerBoundary error_info);
	//! Produces error message when we detect an invalid utf-8 value
	static CSVError InvalidUTF8(const CSVReaderOptions &options, LinesPerBoundary error_info);
	idx_t GetBoundaryIndex() {
		return error_info.boundary_idx;
	}

	//! Actual error message
	string error_message;
	//! Error Type
	CSVErrorType type;
	//! Column Index where error happened
	idx_t column_idx;
	//! Values from the row where error happened
	vector<Value> row;
	//! Line information regarding this error
	LinesPerBoundary error_info;
};

class CSVErrorHandler {
public:
	CSVErrorHandler(bool ignore_errors = false);
	//! Throws the error
	void Error(CSVError csv_error, bool force_error = false);
	//! If we have a cached error, and we can now error, we error.
	void ErrorIfNeeded();
	//! Inserts a finished error info
	void Insert(idx_t boundary_idx, idx_t rows);
	//! Method that actually throws the error
	void ThrowError(CSVError csv_error);
	//! If we processed all boundaries before the one that error-ed
	bool CanGetLine(idx_t boundary_index);
	//! Return the 1-indexed line number
	idx_t GetLine(const LinesPerBoundary &error_info);
	void NewMaxLineSize(idx_t scan_line_size);
	//! Set of errors
	map<LinesPerBoundary, vector<CSVError>> errors;

	idx_t GetMaxLineLength() {
		return max_line_length;
	}
	void DontPrintErrorLine() {
		print_line = false;
	}

private:
	//! If we should print the line of an error
	bool PrintLineNumber(CSVError &error);
	//! CSV Error Handler Mutex
	mutex main_mutex;
	//! Map of <boundary indexes> -> lines read
	unordered_map<idx_t, LinesPerBoundary> lines_per_batch_map;
	idx_t max_line_length = 0;
	bool ignore_errors = false;
	bool print_line = true;
};

} // namespace duckdb
