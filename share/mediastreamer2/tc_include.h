#if (defined TC3000_Z1) \
|| (defined TC3000_T1) \
|| (defined TC3000_R1) \
|| (defined TC3000_W2) \
|| (defined TC3000_W1) \
|| (defined TC3000_X1)

#include "3000_indoor_no_touch/config.h"
#include "3000_indoor_no_touch/tc_protocol/public.h"
#include "3000_indoor_no_touch/tc_protocol/protocol.h"
#include "3000_indoor_touch/tc_protocol/queue.h"
#endif


#if (defined TC3000_Z1_CM) \
|| (defined TC3000_18D1) \
|| (defined TC3000_W2_CM) \
|| (defined TC3000_W1_CM) \
|| (defined TC3000_P1_CM) 
#include "3000_indoor_touch/config.h"
#include "3000_indoor_touch/tc_protocol/public.h"
#include "3000_indoor_touch/tc_protocol/protocol.h"
#include "3000_indoor_touch/tc_protocol/queue.h"
#endif

#if (defined TC3100_18D1)
#include "3100_indoor_touch/config.h"
#include "3100_indoor_touch/tc_protocol/protocol.h"
#include "3000_indoor_touch/tc_protocol/queue.h"
#endif
