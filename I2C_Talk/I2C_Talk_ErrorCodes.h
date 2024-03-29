#pragma once
#include <Arduino.h>

namespace I2C_Talk_ErrorCodes {
	enum error_codes { _OK, _Insufficient_data_returned, _NACK_during_address_send, _NACK_during_data_send, _NACK_during_complete, _NACK_receiving_data, _Timeout, _StopMarginTimeout, _SlaveByteProcessTimeout, _BusReleaseTimeout, _I2C_ClockHungLow, _disabledDevice, _slave_shouldnt_write, _I2C_Device_Not_Found, _I2C_ReadDataWrong, _I2C_AddressOutOfRange, _unknownError };
	inline error_codes operator |=(error_codes lhs, error_codes rhs) { return max(lhs, rhs); }
}