#define GPID                            0xa3cf

#define SCOPE_INVERTER                  0x01
#define SCOPE_CAES                      0x02
#define SCOPE_CONTROL                   0x00

#define OFFSET_GPID                     0
#define OFFSET_SCOPE                    2
#define OFFSET_COMPONENTS               3
#define OFFSET_SEQNUM                   4
#define OFFSET_TYPECODE                 6
#define OFFSET_DATA                     8

#define MISDIRECTED_MESSAGE             0x0000
#define UNIMPLEMENTED_PROTOCOL          0x0001
#define UNIMPLEMENTED_FUNCTION          0x0002
#define BAD_UDP_CHECKSUM                0x0003
#define ACK                             0x0004

#define PHASE_ADJUST                    0x1000
#define AMPLITUDE_ADJUST                0x1001
#define REAL_POWER_ADJUST               0x1002
#define REACTIVE_POWER_ADJUST           0x1003
#define FREQUENCY_ADJUST                0x1004

#define SET_MODE_GRID_FOLLOWING         0x2000
#define SET_MODE_STANDALONE             0x2001
#define DISCONNECT                      0x2002
#define CONNECT                         0x2003
#define LOCK_SCREEN                     0x2004
#define UNLOCK_SCREEN                   0x2005
#define SCREEN1                         0x2006
#define SCREEN2                         0x2007
#define SCREEN3                         0x2008
#define SCREEN4                         0x2009

#define REQUEST_PHASE                   0x3000
#define REQUEST_AMPLITUDE               0x3001
#define REQUEST_MODE                    0x3002
#define REQUEST_PWM_PERIOD              0x3003
#define REQUEST_NSAMPLES                0x3004
#define REQUEST_OUTPUT_VOLTAGE          0x3005
#define REQUEST_OUTPUT_CURRENT          0x3006
#define REQUEST_REAL_POWER              0x3007
#define REQUEST_REACTIVE_POWER          0x3008

#define RESPONSE_PHASE                  0x4000
#define RESPONSE_AMPLITUDE              0x4001
#define RESPONSE_MODE                   0x4002
#define RESPONSE_PWM_PERIOD             0x4003
#define RESPONSE_NSAMPLES               0x4004
#define RESPONSE_OUTPUT_VOLTAGE         0x4005
#define RESPONSE_OUTPUT_CURRENT         0x4006
#define RESPONSE_REAL_POWER             0x4007
#define RESPONSE_REACTIVE_POWER         0x4008
#define RESPONSE_FREQUENCY              0x4009

#define GENERAL_TYPE                    1
#define ADJUST_TYPE                     2
#define CONTROL_TYPE                    3
#define REQUEST_TYPE                    4
#define RESPONSE_TYPE                   5
