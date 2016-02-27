
#include "common.h"


void i2c_set_tx_mode(I2C_MemMapPtr p)
{
    p->C1 |= I2C_C1_TX_MASK;

}
void i2c_set_rx_mode(I2C_MemMapPtr p)
{
    p->C1 &= ~I2C_C1_TX_MASK;
}

void i2c_set_slave_mode(I2C_MemMapPtr p)
{
    p->C1  &= ~I2C_C1_MST_MASK;
}
void i2c_set_master_mode(I2C_MemMapPtr p)
{
    p->C1  |=  I2C_C1_MST_MASK;
}

// i2c general

void i2c_give_nack(I2C_MemMapPtr p)
{
    p->C1 |= I2C_C1_TXAK_MASK;
}
void i2c_give_ack(I2C_MemMapPtr p)
{
    p->C1 &= ~I2C_C1_TXAK_MASK;
}
void i2c_repeated_start(I2C_MemMapPtr p)
{
    p->C1     |= 0x04;
}
void i2c_write_byte(I2C_MemMapPtr p, uint8 data)
{
    p->D = data;
}
uint8 i2c_read_byte(I2C_MemMapPtr p)
{
    return p->D;
}
void i2c_start(I2C_MemMapPtr p)
{
    i2c_set_master_mode(p);
    i2c_set_tx_mode(p);
}
void i2c_stop(I2C_MemMapPtr p)
{
    i2c_set_slave_mode(p);
    i2c_set_rx_mode(p);
}
void i2c_wait(I2C_MemMapPtr p)
{
		uint32_t tOut = 0x10000000;
    // wait flag
    while(((p->S & I2C_S_IICIF_MASK)==0) && tOut--);
    // clear flag
    p->S |= I2C_S_IICIF_MASK;
}
uint16 i2c_get_ack(I2C_MemMapPtr p)
{
    if((p->S & I2C_S_RXAK_MASK) == 0)
        return TRUE;
    else
        return FALSE;
}

// -------------------------------------------------
void hal_i2c_init(I2C_MemMapPtr p)
{
    SIM_SCGC4 |= SIM_SCGC4_I2C0_MASK;
	
	 // configure GPIO for I2C function
    PORTE_PCR24 = PORT_PCR_MUX(5);
    PORTE_PCR25 = PORT_PCR_MUX(5);

    
    p->F  = 0x14; // baudrate
    p->C1 = 0x80; // enable IIC
}

void hal_i2c_deinit(I2C_MemMapPtr p)
{
    p->C1 = 0x00;
    
    SIM_SCGC4 &= ~SIM_SCGC4_I2C1_MASK;
}



