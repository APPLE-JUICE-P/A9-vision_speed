#include "Commulink.hpp"
#include "Basic.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "SensorsBackend.hpp"
#include "MeasurementSystem.hpp"

#define SensorInd 2

struct DriverInfo
{
	uint32_t param;
	Port port;
};

typedef struct
{
	int16_t Dist;	//Dist���루30-1200cm��
	int16_t Strength;	//Strength�ź�ǿ�ȣ�20-2000���ţ�
	uint8_t Mode;	//Mode��൵λ
	uint8_t Rsv;	//Ԥ��
}__PACKED _TfMini;
static const unsigned char packet_ID[2] = { 0x59 , 0x59 };

static void TFMini_Server(void* pvParameters)
{
	DriverInfo driver_info = *(DriverInfo*)pvParameters;
	delete (DriverInfo*)pvParameters;
	
	/*״̬��*/
		_TfMini  SensorD;
		unsigned char rc_counter = 0;
		unsigned char sum = 0;
	/*״̬��*/
	
	while(1)
	{
		uint8_t rdata;
		if( driver_info.port.read( &rdata, 1, 2, 0.5 ) )
		{
			if( rc_counter == 0 )
				sum = 0;
			if( rc_counter < 2 )
			{
				//���հ�ͷ
				if( rdata != packet_ID[ rc_counter ] )
					rc_counter = 0;
				else
				{
					sum += rdata;
					++rc_counter;
				}
			}
			else if( rc_counter < 8 )
			{	//��������
				( (unsigned char*)&SensorD )[ rc_counter - 2 ] = rdata;
				sum += (unsigned char)rdata;
				++rc_counter;
			}
			else
			{	//У��
				if( sum == rdata )
				{	//У��ɹ�
					if( SensorD.Strength>20 && SensorD.Dist>1 && SensorD.Dist<5000 )
					{
						vector3<double> position;
						position.z = SensorD.Dist;
						//��ȡ���
						Quaternion quat;
						get_Airframe_quat( &quat );
						double lean_cosin = quat.get_lean_angle_cosin();
						//����
						position.z *= lean_cosin;
						PositionSensorUpdatePosition( SensorInd, position, true );
					}
					else
						PositionSensorSetInavailable( SensorInd );
				}
				rc_counter = 0;
			}
		}
	}
}

static bool TFMini_DriverInit( Port port, uint32_t param )
{
	//������115200
	port.SetBaudRate( 115200, 2, 2 );
	//ע�ᴫ����
	bool res = PositionSensorRegister( SensorInd , \
																			Position_Sensor_Type_RangePositioning , \
																			Position_Sensor_DataType_s_z , \
																			Position_Sensor_frame_ENU , \
																			0.05 , //��ʱ
																			0 ,	//xy���ζ�
																			0 //z���ζ�
																			);
	if(!res)
		return false;
	DriverInfo* driver_info = new DriverInfo;
	driver_info->param = param;
	driver_info->port = port;
	xTaskCreate( TFMini_Server, "OpticalFlow", 1024, (void*)driver_info, SysPriority_ExtSensor, NULL);
	return true;
}

void init_drv_TFMini()
{
	PortFunc_Register( 52, TFMini_DriverInit );
}