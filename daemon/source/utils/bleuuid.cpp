/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2017-2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

//
//  bleuuid.cpp
//  SkyBluetoothRcu
//

#include "bleuuid.h"

#include <QDebug>


// -----------------------------------------------------------------------------
/*!
	\internal

 */
void BleUuid::registerType()
{
	static bool initDone = false;
	if (Q_UNLIKELY(!initDone)) {
		qRegisterMetaType<BleUuid>();
		initDone = true;
	}
}

// The following is the base UUID for all standardised bluetooth APIs
//   {00000000-0000-1000-8000-00805F9B34FB}
#define BASE_UUID_w1   0x0000
#define BASE_UUID_w2   0x1000
#define BASE_UUID_b1   0x80
#define BASE_UUID_b2   0x00
#define BASE_UUID_b3   0x00
#define BASE_UUID_b4   0x80
#define BASE_UUID_b5   0x5F
#define BASE_UUID_b6   0x9B
#define BASE_UUID_b7   0x34
#define BASE_UUID_b8   0xFB

// The following is the base UUID for sky defined bluetooth APIs
//   {00000000-BDF0-407C-AAFF-D09967F31ACD}
#define SKY_UUID_w1    0xBDF0
#define SKY_UUID_w2    0x407C
#define SKY_UUID_b1    0xAA
#define SKY_UUID_b2    0xFF
#define SKY_UUID_b3    0xD0
#define SKY_UUID_b4    0x99
#define SKY_UUID_b5    0x67
#define SKY_UUID_b6    0xF3
#define SKY_UUID_b7    0x1A
#define SKY_UUID_b8    0xCD


// -----------------------------------------------------------------------------
/*!
	Constructs a new null Bluetooth UUID.

 */
BleUuid::BleUuid()
{
	registerType();
}

BleUuid::BleUuid(ServiceType uuid)
	: QUuid(uuid, BASE_UUID_w1, BASE_UUID_w2,
	              BASE_UUID_b1, BASE_UUID_b2, BASE_UUID_b3, BASE_UUID_b4,
	              BASE_UUID_b5, BASE_UUID_b6, BASE_UUID_b7, BASE_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(CharacteristicType uuid)
	: QUuid(uuid, BASE_UUID_w1, BASE_UUID_w2,
	              BASE_UUID_b1, BASE_UUID_b2, BASE_UUID_b3, BASE_UUID_b4,
	              BASE_UUID_b5, BASE_UUID_b6, BASE_UUID_b7, BASE_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(DescriptorType uuid)
	: QUuid(uuid, BASE_UUID_w1, BASE_UUID_w2,
	              BASE_UUID_b1, BASE_UUID_b2, BASE_UUID_b3, BASE_UUID_b4,
	              BASE_UUID_b5, BASE_UUID_b6, BASE_UUID_b7, BASE_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(SkyServiceType uuid)
	: QUuid(uuid, SKY_UUID_w1, SKY_UUID_w2,
	              SKY_UUID_b1, SKY_UUID_b2, SKY_UUID_b3, SKY_UUID_b4,
	              SKY_UUID_b5, SKY_UUID_b6, SKY_UUID_b7, SKY_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(SkyCharacteristicType uuid)
	: QUuid(uuid, SKY_UUID_w1, SKY_UUID_w2,
	              SKY_UUID_b1, SKY_UUID_b2, SKY_UUID_b3, SKY_UUID_b4,
	              SKY_UUID_b5, SKY_UUID_b6, SKY_UUID_b7, SKY_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(SkyDescriptorType uuid)
	: QUuid(uuid, SKY_UUID_w1, SKY_UUID_w2,
	              SKY_UUID_b1, SKY_UUID_b2, SKY_UUID_b3, SKY_UUID_b4,
	              SKY_UUID_b5, SKY_UUID_b6, SKY_UUID_b7, SKY_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(quint16 uuid)
	: QUuid(uuid, BASE_UUID_w1, BASE_UUID_w2,
	              BASE_UUID_b1, BASE_UUID_b2, BASE_UUID_b3, BASE_UUID_b4,
	              BASE_UUID_b5, BASE_UUID_b6, BASE_UUID_b7, BASE_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(quint32 uuid)
	: QUuid(uuid, BASE_UUID_w1, BASE_UUID_w2,
	              BASE_UUID_b1, BASE_UUID_b2, BASE_UUID_b3, BASE_UUID_b4,
	              BASE_UUID_b5, BASE_UUID_b6, BASE_UUID_b7, BASE_UUID_b8)
{
	registerType();
}

BleUuid::BleUuid(const QString &uuid)
	: QUuid(uuid)
{
	registerType();
}

BleUuid::BleUuid(const BleUuid &uuid)
	: QUuid(uuid)
{
	registerType();
}

BleUuid::BleUuid(const QUuid &uuid)
	: QUuid(uuid)
{
	registerType();
}

// -----------------------------------------------------------------------------
/*!
	Destroys the Bluetooth UUID.
 */
BleUuid::~BleUuid()
{
}

// -----------------------------------------------------------------------------
/*!
	Android API to create a new BleUuid from a java.util.UUID object.

 */
#if defined(Q_OS_ANDROID)
BleUuid BleUuid::fromJavaUUID(const QAndroidJniObject &uuid)
{
	BleUuid bleUuid;
	if (!uuid.isValid())
		return bleUuid;

	const jlong msb = uuid.callMethod<jlong>("getMostSignificantBits");
	const jlong lsb = uuid.callMethod<jlong>("getLeastSignificantBits");

	bleUuid.data1 = uint((msb >> 32) & 0xffffffff);
	bleUuid.data2 = ushort((msb >> 16) & 0xffff);
	bleUuid.data3 = ushort((msb >> 0)  & 0xffff);

	bleUuid.data4[0] = uchar((lsb >> 56) & 0xff);
	bleUuid.data4[1] = uchar((lsb >> 48) & 0xff);
	bleUuid.data4[2] = uchar((lsb >> 40) & 0xff);
	bleUuid.data4[3] = uchar((lsb >> 32) & 0xff);
	bleUuid.data4[4] = uchar((lsb >> 24) & 0xff);
	bleUuid.data4[5] = uchar((lsb >> 16) & 0xff);
	bleUuid.data4[6] = uchar((lsb >> 8)  & 0xff);
	bleUuid.data4[7] = uchar((lsb >> 0)  & 0xff);

	return bleUuid;
}

BleUuid BleUuid::fromJavaUUID(jobject object)
{
	return fromJavaUUID(QAndroidJniObject(object));
}
#endif // defined(Q_OS_ANDROID)

// -----------------------------------------------------------------------------
/*!
	Returns \c true if \a other is equal to this Bluetooth UUID, otherwise
	\c false.

 */
bool BleUuid::operator==(const BleUuid &other) const
{
	return QUuid::operator==(other);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if \a other is equal to this Bluetooth UUID, otherwise
	\c false.

 */
bool BleUuid::operator==(const QUuid &other) const
{
	return QUuid::operator==(other);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the uuid is in the bluetooth consortium's standard uuid
	range, i.e. it is not a vendor defined uuid.

 */
bool BleUuid::isStandard() const
{
	static const quint8 baseData4[8] = { BASE_UUID_b1, BASE_UUID_b2,
	                                     BASE_UUID_b3, BASE_UUID_b4,
	                                     BASE_UUID_b5, BASE_UUID_b6,
	                                     BASE_UUID_b7, BASE_UUID_b8 };

	return (data2 == BASE_UUID_w1) &&
	       (data3 == BASE_UUID_w2) &&
	       (memcmp(baseData4, data4, 8) == 0);
}

// -----------------------------------------------------------------------------
/*!
	Returns \c true if the uuid is in the bluetooth consortium's standard uuid
	range, i.e. it is not a vendor defined uuid.

 */
bool BleUuid::isSkyDefined() const
{
	static const quint8 skyData4[8]  = { SKY_UUID_b1, SKY_UUID_b2,
	                                     SKY_UUID_b3, SKY_UUID_b4,
	                                     SKY_UUID_b5, SKY_UUID_b6,
	                                     SKY_UUID_b7, SKY_UUID_b8 };

	return (data2 == SKY_UUID_w1) &&
	       (data3 == SKY_UUID_w2) &&
	       (memcmp(skyData4, data4, 8) == 0);
}

// -----------------------------------------------------------------------------
/*!
	Returns the name of the service / characteristic or descriptor that the
	uuid corresponds to if known, otherwise an empty string.

 */
QString BleUuid::longName() const
{
	if (isSkyDefined()) {
		switch (data1) {
			case SkyQVoice:                 return QStringLiteral("com.sky.service.skyq_voice");
			case SkyQInfrared:              return QStringLiteral("com.sky.service.skyq_infrared");
			case SkyQFirmwareUpgrade:       return QStringLiteral("com.sky.service.skyq_firmware_upgrade");

			case AudioCodecs:               return QStringLiteral("com.sky.characteristic.audio_codecs");
			case AudioGain:                 return QStringLiteral("com.sky.characteristic.audio_gain");
			case AudioControl:              return QStringLiteral("com.sky.characteristic.audio_control");
			case AudioData:                 return QStringLiteral("com.sky.characteristic.audio_data");
			case InfraredStandby:           return QStringLiteral("com.sky.characteristic.infrared_standby");
			case InfraredCodeId:            return QStringLiteral("com.sky.characteristic.infrared_code_id");
			case InfraredSignal:            return QStringLiteral("com.sky.characteristic.infrared_signal");
			case EmitInfraredSignal:        return QStringLiteral("com.sky.characteristic.infrared_emit");
			case FirmwareControlPoint:      return QStringLiteral("com.sky.characteristic.firmware_control_point");
			case FirmwarePacket:            return QStringLiteral("com.sky.characteristic.firmware_packet");

			case InfraredSignalReference:   return QStringLiteral("com.sky.descriptor.infrared_signal_reference");
			case InfraredSignalConfiguration: return QStringLiteral("com.sky.descriptor.infrared_signal_configuration");
			case FirmwarePacketWindowSize:  return QStringLiteral("com.sky.descriptor.firmware_packet_window_size");

		}

	} else if (isStandard()) {
		switch (data1) {
			case GenericAccess:             return QStringLiteral("org.bluetooth.service.generic_access");
			case GenericAttribute:          return QStringLiteral("org.bluetooth.service.generic_attribute");
			case ImmediateAlert:            return QStringLiteral("org.bluetooth.service.immediate_alert");
			case DeviceInformation:         return QStringLiteral("org.bluetooth.service.device_information");
			case BatteryService:            return QStringLiteral("org.bluetooth.service.battery_service");
			case HumanInterfaceDevice:      return QStringLiteral("org.bluetooth.service.human_interface_device");
			case LinkLoss:                  return QStringLiteral("org.bluetooth.service.link_loss");
			case TxPower:                   return QStringLiteral("org.bluetooth.service.tx_power");
			case ScanParameters:            return QStringLiteral("org.bluetooth.service.scan_parameters");

			case ScanRefresh:               return QStringLiteral("org.bluetooth.characteristic.scan_refresh");
			case ScanIntervalWindow:        return QStringLiteral("org.bluetooth.characteristic.scan_interval_window");
			case DeviceName:                return QStringLiteral("org.bluetooth.characteristic.gap.device_name");
			case Appearance:                return QStringLiteral("org.bluetooth.characteristic.gap.appearance");
			case ServiceChanged:            return QStringLiteral("org.bluetooth.characteristic.gatt.service_changed");
			case AlertLevel:                return QStringLiteral("org.bluetooth.characteristic.alert_level");
			case BatteryLevel:              return QStringLiteral("org.bluetooth.characteristic.battery_level");
			case SystemID:                  return QStringLiteral("org.bluetooth.characteristic.system_id");
			case ModelNumberString:         return QStringLiteral("org.bluetooth.characteristic.model_number_string");
			case SerialNumberString:        return QStringLiteral("org.bluetooth.characteristic.serial_number_string");
			case FirmwareRevisionString:    return QStringLiteral("org.bluetooth.characteristic.firmware_revision_string");
			case HardwareRevisionString:    return QStringLiteral("org.bluetooth.characteristic.hardware_revision_string");
			case SoftwareRevisionString:    return QStringLiteral("org.bluetooth.characteristic.software_revision_string");
			case ManufacturerNameString:    return QStringLiteral("org.bluetooth.characteristic.manufacturer_name_string");
			case PnPID:                     return QStringLiteral("org.bluetooth.characteristic.pnp_id");
			case BootKeyboardOutputReport:  return QStringLiteral("org.bluetooth.characteristic.boot_keyboard_output_report");
			case BootMouseInputReport:      return QStringLiteral("org.bluetooth.characteristic.boot_mouse_input_report");
			case HIDInformation:            return QStringLiteral("org.bluetooth.characteristic.hid_information");
			case ReportMap:                 return QStringLiteral("org.bluetooth.characteristic.report_map");
			case HIDControlPoint:           return QStringLiteral("org.bluetooth.characteristic.hid_control_point");
			case Report:                    return QStringLiteral("org.bluetooth.characteristic.report");
			case ProtocolMode:              return QStringLiteral("org.bluetooth.characteristic.protocol_mode");
			case IEEERegulatatoryCertificationDataList:
			                                return QStringLiteral("org.bluetooth.characteristic.ieee_11073-20601_regulatory_certification_data_list");
			case PeripheralPreferredConnectionParameters:
			                                return QStringLiteral("org.bluetooth.characteristic.gap.peripheral_preferred_connection_parameters");

			case ClientCharacteristicConfiguration:
			                                return QStringLiteral("org.bluetooth.descriptor.gatt.client_characteristic_configuration");
			case ReportReference:           return QStringLiteral("org.bluetooth.descriptor.report_reference");
		}

	}

	return QString();
}

// -----------------------------------------------------------------------------
/*!
	Returns the name of the service / characteristic or descriptor that the
	uuid corresponds to if known, otherwise an empty string.

 */
QString BleUuid::name() const
{
	if (isSkyDefined()) {
		switch (data1) {
			case SkyQVoice:                 return QStringLiteral("SkyQ Voice");
			case SkyQInfrared:              return QStringLiteral("SkyQ Infrared");
			case SkyQFirmwareUpgrade:       return QStringLiteral("SkyQ Firmware Upgrade");

			case AudioCodecs:               return QStringLiteral("Audio Codecs");
			case AudioGain:                 return QStringLiteral("Audio Gain");
			case AudioControl:              return QStringLiteral("Audio Control");
			case AudioData:                 return QStringLiteral("Audio Data");
			case InfraredStandby:           return QStringLiteral("Infrared Standby");
			case InfraredCodeId:            return QStringLiteral("Infrared CodeId");
			case InfraredSignal:            return QStringLiteral("Infrared Signal");
			case EmitInfraredSignal:        return QStringLiteral("Emit Infrared Signal");
			case FirmwareControlPoint:      return QStringLiteral("Firmware ControlPoint");
			case FirmwarePacket:            return QStringLiteral("Firmware Packet");

			case InfraredSignalReference:   return QStringLiteral("Infrared Signal Reference");
			case InfraredSignalConfiguration: return QStringLiteral("Infrared Signal Configuration");
			case FirmwarePacketWindowSize:  return QStringLiteral("Firmware Packet Window Size");

		}

	} else if (isStandard()) {
		switch (data1) {
			case GenericAccess:             return QStringLiteral("Generic Access");
			case GenericAttribute:          return QStringLiteral("Generic Attribute");
			case ImmediateAlert:            return QStringLiteral("Immediate Alert");
			case DeviceInformation:         return QStringLiteral("Device Information");
			case BatteryService:            return QStringLiteral("Battery Service");
			case HumanInterfaceDevice:      return QStringLiteral("Human Interface Device");
			case LinkLoss:                  return QStringLiteral("Link Loss");
			case TxPower:                   return QStringLiteral("TX Power");
			case ScanParameters:            return QStringLiteral("Scan Parameters");

			case ScanRefresh:               return QStringLiteral("Scan Refresh");
			case ScanIntervalWindow:        return QStringLiteral("Scan Interval Window");
			case DeviceName:                return QStringLiteral("Device Name");
			case Appearance:                return QStringLiteral("Appearance");
			case ServiceChanged:            return QStringLiteral("Service Changed");
			case AlertLevel:                return QStringLiteral("Alert Level");
			case BatteryLevel:              return QStringLiteral("Battery Level");
			case SystemID:                  return QStringLiteral("System ID");
			case ModelNumberString:         return QStringLiteral("Model Number");
			case SerialNumberString:        return QStringLiteral("Serial Number");
			case FirmwareRevisionString:    return QStringLiteral("Firmware Revision");
			case HardwareRevisionString:    return QStringLiteral("Hardware Revision");
			case SoftwareRevisionString:    return QStringLiteral("Software Revision");
			case ManufacturerNameString:    return QStringLiteral("Manufacturer Name");
			case PnPID:                     return QStringLiteral("PnP ID");
			case BootKeyboardOutputReport:  return QStringLiteral("Boot Keyboard Output Report");
			case BootMouseInputReport:      return QStringLiteral("Boot MouseInput Report");
			case HIDInformation:            return QStringLiteral("HID Information");
			case ReportMap:                 return QStringLiteral("Report Map");
			case HIDControlPoint:           return QStringLiteral("HID Control Point");
			case Report:                    return QStringLiteral("Report");
			case ProtocolMode:              return QStringLiteral("Protocol Mode");
			case IEEERegulatatoryCertificationDataList:     return QStringLiteral("IEEE Regulatory Certification Data List");
			case PeripheralPreferredConnectionParameters:   return QStringLiteral("Peripheral Preferred Connection Parameters");

			case ClientCharacteristicConfiguration: return QStringLiteral("Client Characteristic Configuration");
			case ReportReference:           return QStringLiteral("Report Reference");
		}

	}

	return QString();
}

// -----------------------------------------------------------------------------
/*!
	An override of the QUuid::toString() method that allows for returning a
	string with or without the curly braces around it.  Typically when used with
	BLE the UUID are displayed with braces.

 */
QString BleUuid::toString(UuidFormat format) const
{
	QString str = QUuid::toString();

	if (format == WithCurlyBraces)
		return str;

	if (!str.isNull()) {
		str.remove('{');
		str.remove('}');
	}

	return str;
}

// -----------------------------------------------------------------------------
/*!
	Debugging macro that prints out the uuid and it's name if it has one.

 */
QDebug operator<<(QDebug debug, const BleUuid &uuid)
{
	QDebugStateSaver saver(debug);
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
	debug.noquote();
#endif
	debug.nospace();

	const QString name = uuid.name();
	if (name.isEmpty())
		debug << uuid.toString(BleUuid::WithoutCurlyBraces);
	else
		debug << uuid.toString(BleUuid::WithoutCurlyBraces) << " [" << name << "]";

	return debug;
}


