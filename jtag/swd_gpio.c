#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// ARM CoreSight SWD-DP packet request values
#define SW_IDCODE_RD            0xA5
#define SW_ABORT_WR             0x81
#define SW_CTRLSTAT_RD          0x8D
#define SW_CTRLSTAT_WR          0xA9
#define SW_RESEND_RD            0x95
#define SW_SELECT_WR            0xB1
#define SW_RDBUFF_RD            0xBD

// ARM CoreSight SW-DP packet request masks
#define SW_REQ_PARK_START       0x81
#define SW_REQ_PARITY           0x20
#define SW_REQ_A32              0x18
#define SW_REQ_RnW              0x04
#define SW_REQ_APnDP            0x02

// ARM CoreSight SW-DP packet acknowledge values
#define SW_ACK_OK               0x1
#define SW_ACK_WAIT             0x2
#define SW_ACK_FAULT            0x4
#define SW_ACK_PARITY_ERR       0x8

// ARM CoreSight DAP command values
#define DAP_IDCODE_RD           0x02
#define DAP_ABORT_WR            0x00
#define DAP_CTRLSTAT_RD         0x06
#define DAP_CTRLSTAT_WR         0x04
#define DAP_SELECT_WR           0x08
#define DAP_RDBUFF_RD           0x0E

// ARM CoreSight DAP command masks
#define DAP_CMD_PACKED          0x80
#define DAP_CMD_A32             0x0C
#define DAP_CMD_RnW             0x02
#define DAP_CMD_APnDP           0x01
#define DAP_CMD_MASK            0x0F


#define DAP_RETRY_COUNT         255

// Command Status Response Codes
#define HOST_COMMAND_OK         0x55
#define HOST_INVALID_COMMAND    0x80
#define HOST_COMMAND_FAILED     0x81
#define HOST_AP_TIMEOUT         0x82
#define HOST_WIRE_ERROR         0x83
#define HOST_ACK_FAULT          0x84
#define HOST_DP_NOT_CONNECTED   0x85





// Cortex M3 Debug Registers (AHB addresses)
#define DDFSR   0xE000ED30      // Debug Fault StatusRegister
#define DHCSR   0xE000EDF0      // Debug Halting Control and Status Register
#define DCRSR   0xE000EDF4      // Debug Core Register Selector Register
#define DCRDR   0xE000EDF8      // Debug Core Register Data Register
#define DEMCR   0xE000EDFC      // Debug Exception and Monitor Control Register
#define AIRCR   0xE000ED0C      // The Application Interrupt and Reset Control Register

//  Cortex M3 Memory Access Port
#define MEMAP_BANK_0  0x00000000       // BANK 0 => CSW, TAR, Reserved, DRW
#define MEMAP_BANK_1  0x00000010       // BANK 1 => BD0, BD1, BD2, BD3

// SiM3 Chip Access Port (SiLabs specific Debug Access Port)
#define CHIPAP_BANK_0  0x0A000000      // BANK 0 => CTRL1, CTRL2, LOCK, CRC
#define CHIPAP_BANK_1  0x0A000010      // BANK 1 => INIT_STAT, DAP_IN, DAP_OUT, None
#define CHIPAP_BANK_F  0x0A0000F0      // BANK F => None, None, None, ID

// MEMAP register addresses
#define MEMAP_CSW  0x01
#define MEMAP_TAR  0x05
#define MEMAP_DRW_WR  0x0D
#define MEMAP_DRW_RD  0x0F

// CHIPAP register addresses
#define CHIPAP_CTRL1_WR     0x01
#define CHIPAP_CTRL2_WR     0x05
#define CHIPAP_ID_WR        0x0D
#define CHIPAP_ID_RD        0x0F

#define MEMAP_ID_RD        0xFF

typedef unsigned int u32;
typedef   signed int s32;

typedef unsigned short u16;
typedef   signed short s16;

typedef unsigned char u8;
typedef   signed char s8;

u8 io_byte;
u32 io_word;

const u8 even_parity[] =
{
    0x00, 0x10, 0x10, 0x00,
    0x10, 0x00, 0x00, 0x10,
    0x10, 0x00, 0x00, 0x10,
    0x00, 0x10, 0x10, 0x00
};

#define GPIO_DIR      "/sys/class/gpio"

#define GPIO_EXPORT   "/sys/class/gpio/export"

#define GPIO_DIR      "/sys/class/gpio"

#define GPIO_EXPORT   "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"

/* BCM GPIO NUM */
#if 0
#define SWD_CLK     (26)
#define SWD_IO      (19)
#else
/* wPi GPIO NUM */
#define SWD_CLK     (25)
#define SWD_IO      (24)

#endif

#define get_bit(x, bit_index) ((x >> bit_index) & 0x1)

void set_bit(u32 *x, u32 bit_index, u32 b)
{
    u32 _x;
    u32 bit_mask;
    _x = *x;
    if (get_bit(_x, bit_index) != b) {
        if (b == 0) {
            bit_mask = ~(0x1 << bit_index);
            *x = (_x) & bit_mask;
        } else {    /* b == 1 */
            bit_mask = (0x1 << bit_index);
            *x = (_x) | bit_mask;
        }
    }
}

void __gpio_init(int gpio_num, char *direct)
{
    char cmd[512];

    snprintf(cmd, sizeof(cmd), "gpio mode %d %s", gpio_num, direct);
    //printf("cmd: %s\n", cmd);
    system(cmd);
}

int gpio_get(int gpio_num)
{
    int i;
    FILE *fp;
    char cmd[512];
    char out[512];

    //__gpio_init(gpio_num, "in");

    memset(out, 0, sizeof(out));
    snprintf(cmd, sizeof(cmd), "gpio read %d", gpio_num);

    fp = popen(cmd, "r");
    fread(out, sizeof(char), sizeof(out), fp);
    /* printf("gpio %d read [%s] \n", gpio_num, out); */

#if 0
    for(int i = 0; i < strlen(out); i++) {
        printf("[%d]: 0x%02x\n", i, out[i]);
    }
#endif

    if (strcmp(out, "0\n") == 0) {
        pclose(fp);
        return 0;
    } else if (strcmp(out, "1\n") == 0) {
        pclose(fp);
        return 1;
    } else {
        printf("error: gpio %d read return [%s]\n", gpio_num, out);

        for(i = 0; i < 10; i++) {
            printf("[%d]: 0x%02x\n", i, out[i]);
        }

        exit(-1);
    }
}

void gpio_set(int gpio_num, int value)
{
    char cmd[512];

    snprintf(cmd, sizeof(cmd), "gpio write %d %d", gpio_num, value);
    system(cmd);
    if (gpio_get(gpio_num) != value) {
        printf("error: gpio %d set %d fail \n", gpio_num, value);
        exit(-1);
    }

}


void gpio_init()
{
    printf("SWD_CLK: gpio mode %d out\n", SWD_CLK);
    __gpio_init(SWD_CLK, "out");
    printf("SWD_IO:  gpio mode %d out\n", SWD_IO);
    __gpio_init(SWD_IO, "out");
}

void gpio_mode(int gpio_num, char *direct)
{
    __gpio_init(gpio_num, direct);
}

void swd_clk()
{
	gpio_set(SWD_CLK, 1);
	//usleep(1);
	gpio_set(SWD_CLK, 0);
	//usleep(1);
}

void SW_ShiftReset(void)
{
    u8 i;

    // Drive SWDIO high
	gpio_set(SWD_IO, 1);

    // Complete 64 SWCLK cycles
    for (i = 64; i != 0; i--)
    {
		swd_clk();
    }
}

void SW_ShiftByteOut(u8 byte)
{
	u32 i;
    // Make sure SWDIO is an output
	//gpio_set(SWD_IO, 1);

    // Shift 8-bits out on SWDIO
    for(i = 0; i < 8; i++) {
        gpio_set(SWD_IO, get_bit(byte, i));
		swd_clk();
    }

}

u8 SW_ShiftByteIn(void)
{
	u32 i;
	u32 byte; /* FIXME: u8 */

	for(i = 0; i < 8; i++) {
		set_bit(&byte, i, gpio_get(SWD_IO));
		swd_clk();
	}

    // Return the byte that was shifted in
    return (u8)byte;
}

u8 SW_CalcDataParity(u8 *byte_array)
{
    u8 parity;

    // Calculate column parity, reducing down to 4 columns
    parity  = byte_array[0];
    parity ^= byte_array[1];
    parity ^= byte_array[2];
    parity ^= byte_array[3];
    parity ^= parity >> 4;

    // Use lookup table to get parity on 4 remaining bits. The cast (bit)
    // converts any non-zero value to 1.
    if (even_parity[parity & 0xF]) {
        return 1;
    } else {
        return 0;
    }
}

u8 ack_error;

u8 SW_ShiftPacket(u8 request, u8 retry)
{
    u8 ack, limit, i;
	u8 b0, b1, b2;
	u8 *byte_array = (u8 *)(&io_word);

    // If retry parameter is zero, use the default value instead
    if (retry == 0)
    {
        retry = DAP_RETRY_COUNT;
    }
    limit = retry;

    // While waiting, do request phase (8-bit request, turnaround, 3-bit ack)
    do
    {
        // Turnaround or idle cycle, makes or keeps SWDIO an output
        gpio_mode(SWD_IO, "out");
		gpio_set(SWD_IO, 0);
		swd_clk();

        // Shift out the 8-bit packet request
        SW_ShiftByteOut(request);

        //printf("%s-%d\n", __func__, __LINE__);
        // Turnaround cycle makes SWDIO an input
		swd_clk();

        //printf("%s-%d\n", __func__, __LINE__);
        // Shift in the 3-bit acknowledge response
        io_byte = 0;

        gpio_mode(SWD_IO, "in");
        b0 = gpio_get(SWD_IO);  swd_clk();
        b1 = gpio_get(SWD_IO);  swd_clk();
        b2 = gpio_get(SWD_IO);  swd_clk();

        set_bit((u32 *)&io_byte, 0, b0);    /* FIXME: u8 */
        set_bit((u32 *)&io_byte, 1, b1);    /* FIXME: u8 */
        set_bit((u32 *)&io_byte, 2, b2);    /* FIXME: u8 */

		ack = io_byte;

        //printf("%s-%d ack %d\n", __func__, __LINE__, ack);

        // Check if we need to retry the request
        if ((ack == SW_ACK_WAIT) && --retry)
        {
            // Delay an increasing amount with each retry
            for (i=retry; i < limit; i++);
        }
        else
        {
            break;  // Request phase complete (or timeout)
        }
    }
    while (1);

    //printf("ack: %d\n", ack);

    // If the request was accepted, do the data transfer phase (turnaround if
    // writing, 32-bit data, and parity)
    if (ack == SW_ACK_OK)
    {
        if (request & SW_REQ_RnW)
        {
            gpio_mode(SWD_IO, "in");
            // Swap endian order while shifting in 32-bits of data
            byte_array[0] = SW_ShiftByteIn();
            byte_array[1] = SW_ShiftByteIn();
            byte_array[2] = SW_ShiftByteIn();
            byte_array[3] = SW_ShiftByteIn();

            // Shift in the parity bit
			b0 = gpio_get(SWD_IO);
			swd_clk();

            // Check for parity error
            if (b0 ^ SW_CalcDataParity(byte_array))
            {
                ack = SW_ACK_PARITY_ERR;
            }
        }
        else
        {
            // Turnaround cycle makes SWDIO an output
            gpio_mode(SWD_IO, "out");
            swd_clk();

            // Swap endian order while shifting out 32-bits of data
            SW_ShiftByteOut(byte_array[0]);
            SW_ShiftByteOut(byte_array[1]);
            SW_ShiftByteOut(byte_array[2]);
            SW_ShiftByteOut(byte_array[3]);

            // Shift out the parity bit
            gpio_set(SWD_IO, get_bit(SW_CalcDataParity(byte_array), 0)); 
            //gpio_set(SWD_IO, 0); 
			swd_clk();
        }
    }
    // TODO: Add error (FAULT, line, parity) handling here?  RESEND on parity error?

    // Turnaround or idle cycle, always leave SWDIO an output
    gpio_mode(SWD_IO, "out");
	gpio_set(SWD_IO, 0);
	swd_clk();

    // Update the global error accumulator if there was an error
    if (ack != SW_ACK_OK)
    {
        ack_error = ack;
    }
    return ack;
}

u8 SW_Response(u8 SW_Ack)
{
    //printf("%s-%d SW_Ack: 0x%x\n", __func__, __LINE__, SW_Ack);
    switch (SW_Ack)
	{
		case SW_ACK_OK:     return HOST_COMMAND_OK;
		case SW_ACK_WAIT:   return HOST_AP_TIMEOUT;
		case SW_ACK_FAULT:  return HOST_ACK_FAULT;
		default:            return HOST_WIRE_ERROR;
	}
}

u8 SWD_LineReset(void)
{
    u8 ack;

    // Complete SWD reset sequence (50 cycles high followed by 2 or more idle cycles)
    SW_ShiftReset();
    SW_ShiftByteOut(0);

    //printf("%s-%d\n", __func__, __LINE__);
    // Now read the DPIDR register to move the SWD out of reset
    ack = SW_ShiftPacket(SW_IDCODE_RD, 1);

    //printf("%s-%d %d\n", __func__, __LINE__, ack);

    SW_ShiftByteOut(0);

    //printf("%s-%d\n", __func__, __LINE__);

    return SW_Response(ack);
}

u8 SW_Request(u8 DAP_Addr)
{
    u8 req;

    // Convert the DAP address into a SWD packet request value
    req = DAP_Addr & DAP_CMD_MASK;      // mask off the bank select bits
    req = req | even_parity[req];       // compute and add parity bit
    req = req << 1;                     // move address/parity bits
    req = req | SW_REQ_PARK_START;      // add start and park bits
    return req;
}

void SW_DAP_Read(u8 cnt, u8 DAP_Addr, u32 * read_data)
{
    u8 req;

    // Format the packet request header
    req = SW_Request(DAP_Addr);

    // Shift the first packet and if DP access, send the results
    SW_ShiftPacket(req, 0);
    if (!(req & SW_REQ_APnDP))
    {
        *read_data = io_word;
        read_data++;
    }

    // Perform the requested number of reads
    for (; cnt != 0; cnt--)
    {
        SW_ShiftPacket(req, 0);
        *read_data = io_word;
        read_data++;
    }

    // For AP access, get and send results of the last read
    if (req & SW_REQ_APnDP)
    {
        SW_ShiftPacket(SW_RDBUFF_RD, 0);
        *read_data = io_word;
        read_data++;
    }
}

void SW_DAP_Write(u8 cnt, u8 DAP_Addr, u32 * write_data, u8 final)
{
    u8 req;

    // Format the packet request header
    req = SW_Request(DAP_Addr);

    // Perform the requested number of writes
    do
    {
        //printf("%s-%d\n", __func__, __LINE__);
        io_word = *write_data;
        write_data++;
#if 0
    	// Clear the upper half word for 16-bit packed writes
   		io_word.U16[MSB] = 0;

        // For packed transfers, write 16-bits at a time
        if (DAP_Addr & DAP_CMD_PACKED)
        {
            SW_ShiftPacket(req, 0);
            io_word.U32 = *write_data;
            write_data++;

    		// Clear the upper half word for 16-bit packed writes
   			io_word.U16[MSB] = 0;
        }
        else
        {
            io_word.U16[MSB] = (U16) *write_data;
            write_data++;
        }
#endif
        SW_ShiftPacket(req, 0);
    }
    while (cnt-- != 0);

    // For AP access, check results of last write (use default retry count
    // because previous write may need time to complete)
    if (final && (req & SW_REQ_APnDP))
    {
        SW_ShiftPacket(SW_RDBUFF_RD, 0);
    }
}

u8 SWD_DAP_Move(u8 cnt, u8 dap, u32 * transfer_data)
{
    u8 resp;
    // Reset global error accumulator
    ack_error = SW_ACK_OK;

    // Determine if this is a read or write transfer
    if (dap & DAP_CMD_RnW)
    {
        // Perform the requested number of reads
        printf("DAP Read\n");
        SW_DAP_Read(cnt, dap, transfer_data);
    }
    else
    {
        printf("DAP Write\n");
        SW_DAP_Write(cnt, dap, transfer_data, 1);
    }

    // Finish with idle cycles
    SW_ShiftByteOut(0);

    // Return the accumulated error result
    resp = SW_Response(ack_error);
    printf("%s resp: 0x%x\n", __func__, resp);
    return resp;
}

u8 SWD_ClearErrors(void)
{
    u8 ack;

    // First read the DP-CSR register and send the value to the host.
    SW_ShiftPacket(SW_CTRLSTAT_RD, 1);
    //SendLongToHost(io_word.U32);

    // Clear all error/sticky bits by writing to the abort register.
    io_word = 0x1E;
    SW_ShiftPacket(SW_ABORT_WR, 1);

    // Read the DP-CSR register again and send the results to the host.
    ack = SW_ShiftPacket(SW_CTRLSTAT_RD, 1);
    SW_ShiftByteOut(0);
    //SendLongToHost(io_word.U32);

    return SW_Response(ack);
}

void target_write(u32 addr, u32 data)
{
    SWD_DAP_Move(0, MEMAP_TAR, &addr);
    SWD_DAP_Move(0, MEMAP_DRW_WR, &data);
}

void connect_and_halt_core()
{
    u32 rw_data;

#if 0
    rw_data = CHIPAP_BANK_F;
    SWD_DAP_Move(0, DAP_SELECT_WR, &rw_data);
    SWD_DAP_Move(0, CHIPAP_ID_RD, &rw_data);

    if (rw_data != 0x2430002) {
        printf("%s-%d: rw_data: 0x%x\n", __func__, __LINE__, rw_data);
        return;
    }

    // CTRL1.core_reset_ap = 1
    rw_data = CHIPAP_BANK_0;
    SWD_DAP_Move(0, DAP_SELECT_WR, &rw_data);
    rw_data = 0x08;
    SWD_DAP_Move(0, CHIPAP_CTRL1_WR, &rw_data);
#endif

    // Select MEM BANK 0
    rw_data = MEMAP_BANK_0;
    SWD_DAP_Move(0, DAP_SELECT_WR, &rw_data);

    // 32 bit memory access, auto increment
    rw_data = 0x23000002;
    SWD_DAP_Move(0, MEMAP_CSW, &rw_data);

    // DHCSR.C_DEBUGEN = 1
    rw_data = DHCSR;
    SWD_DAP_Move(0, MEMAP_TAR, &rw_data);
    rw_data = 0xA05F0001;
    SWD_DAP_Move(0, MEMAP_DRW_WR, &rw_data);

    // DEMCR.VC_CORERESET = 1
    rw_data = DEMCR;
    SWD_DAP_Move(0, MEMAP_TAR, &rw_data);
    rw_data = 0x1;
    SWD_DAP_Move(0, MEMAP_DRW_WR, &rw_data);

    while(1) {
        printf("---------------%s-%d-------------\n", __func__, __LINE__);
        target_write(0x40010C0C, 0xFFFFFFFF);
        usleep(1000000);

        target_write(0x40010C0C, 0x0);
        usleep(1000000);
    }

    // reset the core
    rw_data = AIRCR;
    SWD_DAP_Move(0, MEMAP_TAR, &rw_data);
    rw_data = 0xFA050004;
    SWD_DAP_Move(0, MEMAP_DRW_WR, &rw_data);

    // CTRL1.core_reset_ap = 0
    rw_data = CHIPAP_BANK_0;
    SWD_DAP_Move(0, DAP_SELECT_WR, &rw_data);
    rw_data = 0;
    SWD_DAP_Move(0, CHIPAP_CTRL1_WR, &rw_data);

    // Select MEM BANK 0
    rw_data = MEMAP_BANK_0;
    SWD_DAP_Move(0, DAP_SELECT_WR, &rw_data);
}


int main()
{
    u8 rv;
    u32 idcode;
    u32 transfer_data;

    gpio_init();
    printf("%s-%d\n", __func__, __LINE__);

    SW_ShiftReset();
    printf("%s-%d\n", __func__, __LINE__);

    SW_ShiftByteOut(0x9E);
    printf("%s-%d\n", __func__, __LINE__);

    SW_ShiftByteOut(0xE7);
    printf("%s-%d\n", __func__, __LINE__);

    rv = SWD_LineReset();
    printf("%s-%d\n", __func__, __LINE__);

    printf("rv: 0x%x\n", rv);

	SWD_DAP_Move(0, DAP_IDCODE_RD, &idcode);

    printf("idcode: [0x%08x]\n", idcode);


    printf("%s-%d\n", __func__, __LINE__);

    //transfer_data = 0;
    //SWD_DAP_Move(0, DAP_CTRLSTAT_RD, &transfer_data);
    //printf("%s-%d DAP_CTRLSTAT: 0x%x\n", __func__, __LINE__, transfer_data);

    io_word = 0x1E;
    SW_ShiftPacket(SW_ABORT_WR, 1);

    transfer_data = 0x00;   /* select AP-0x0, BANK-0x00 */
    SWD_DAP_Move(0, DAP_SELECT_WR, &transfer_data);
    printf("%s-%d \n", __func__, __LINE__);

    transfer_data = 0x50000000;
    SWD_DAP_Move(0, DAP_CTRLSTAT_WR, &transfer_data);
    printf("%s-%d\n", __func__, __LINE__);

    transfer_data = 0;
    SWD_DAP_Move(0, DAP_CTRLSTAT_RD, &transfer_data);
    printf("%s-%d DAP_CTRLSTAT: 0x%x\n", __func__, __LINE__, transfer_data);

    transfer_data = 0;
    SWD_DAP_Move(0, DAP_CTRLSTAT_RD, &transfer_data);
    printf("%s-%d DAP_CTRLSTAT: 0x%x\n", __func__, __LINE__, transfer_data);

    transfer_data = 0x50000F00;
    SWD_DAP_Move(0, DAP_CTRLSTAT_WR, &transfer_data);
    printf("%s-%d\n", __func__, __LINE__);


#if 0
    transfer_data = 0x00;   /* select AP-0x0, BANK-0x00 */
    SWD_DAP_Move(0, DAP_SELECT_WR, &transfer_data);
    printf("%s-%d \n", __func__, __LINE__);

    transfer_data = 0x50000000;
    //transfer_data = 0xf0000041;
    SWD_DAP_Move(0, DAP_CTRLSTAT_WR, &transfer_data);
    printf("%s-%d\n", __func__, __LINE__);

    transfer_data = 0;
    SWD_DAP_Move(0, DAP_CTRLSTAT_RD, &transfer_data);
    printf("%s-%d DAP_CTRLSTAT: 0x%x\n", __func__, __LINE__, transfer_data);


    //transfer_data = 0x54000000;
    //SWD_DAP_Move(0, DAP_CTRLSTAT_WR, &transfer_data);
    //printf("%s-%d\n", __func__, __LINE__);

    transfer_data = 0x00;   /* select AP-0x0, BANK-0x0 */
    SWD_DAP_Move(0, DAP_SELECT_WR, &transfer_data);
    printf("%s-%d \n", __func__, __LINE__);
#endif

    transfer_data = 0xF0;   /* select AP-0x0, BANK-0x0F */
    SWD_DAP_Move(0, DAP_SELECT_WR, &transfer_data);
    printf("%s-%d \n", __func__, __LINE__);

    transfer_data = 0x00;
    SWD_DAP_Move(0, MEMAP_ID_RD, &transfer_data);
    printf("%s-%d MEMAP_ID_RD: 0x%x\n", __func__, __LINE__, transfer_data);

    while(1);
    //transfer_data = 0x00;
    //SWD_DAP_Move(0, MEMAP_CSW, &transfer_data);

    //transfer_data = 0;
    //SWD_DAP_Move(0, 0xC, &transfer_data);
    //SWD_DAP_Move(0, 0xC, &transfer_data);
    //printf("%s-%d data: 0x%x\n", __func__, __LINE__, transfer_data);

    //rv = SWD_ClearErrors();

    //printf("%s-%d rv: 0x%x\n", __func__, __LINE__, rv);

    connect_and_halt_core();
    printf("%s-%d\n", __func__, __LINE__);

    while(1) {
        target_write(0x40010C0C, 0xFFFFFFFF);
        usleep(1000000);

        target_write(0x40010C0C, 0x0);
        usleep(1000000);
    }

    return 0;
}