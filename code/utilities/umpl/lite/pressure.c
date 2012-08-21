/*
 $License:
    Copyright (C) 2011 InvenSense Corporation, All Rights Reserved.
 $
 */
/*******************************************************************************
 *
 * $Id: pressure.c 4120 2010-11-21 19:56:16Z mcaramello $
 *
 *******************************************************************************/

/** 
 *  @defgroup PRESSUREDL 
 *  @brief  Motion Library - Pressure Driver Layer.
 *          Provides the interface to setup and handle a pressure sensor
 *          connected to either the primary or the seconday I2C interface 
 *          of the gyroscope.
 *
 *  @{
 *      @file   pressure.c
 *      @brief  Pressure setup and handling methods.
**/

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#include <string.h>

#include "pressure.h"

#include "ml.h"
#include "mlinclude.h"
#include "dmpKey.h"
#include "mlFIFO.h"
#include "mldl.h"
#include "mldl_cfg.h"
#include "mlMathFunc.h"
#include "mlsl.h"
#include "mlos.h"

#include "log.h"
#undef MPL_LOG_TAG
#define MPL_LOG_TAG "MPL-pressure"

#define _pressureDebug(x)       //{x}

/* --------------------- */
/* - Global Variables. - */
/* --------------------- */

/* --------------------- */
/* - Static Variables. - */
/* --------------------- */

/* --------------- */
/* - Prototypes. - */
/* --------------- */

/* -------------- */
/* - Externs.   - */
/* -------------- */

/* -------------- */
/* - Functions. - */
/* -------------- */

/** 
 *  @brief  Is a pressure configured and used by MPL?
 *  @return INV_SUCCESS if the pressure is present.
 */
unsigned char inv_pressure_present(void)
{
    INVENSENSE_FUNC_START;
    struct mldl_cfg *mldl_cfg = inv_get_dl_config();
    if (mldl_cfg->slave[EXT_SLAVE_TYPE_PRESSURE] &&
        mldl_cfg->slave[EXT_SLAVE_TYPE_PRESSURE]->resume &&
        mldl_cfg->inv_mpu_cfg->requested_sensors & INV_THREE_AXIS_PRESSURE)
        return true;
    else
        return false;
}

/**
 *  @brief   Query the pressure slave address.
 *  @return  The 7-bit pressure slave address.
 */
unsigned char inv_get_pressure_slave_addr(void)
{
    INVENSENSE_FUNC_START;
    struct mldl_cfg *mldl_cfg = inv_get_dl_config();
    if (mldl_cfg->pdata_slave[EXT_SLAVE_TYPE_PRESSURE])
        return mldl_cfg->pdata_slave[EXT_SLAVE_TYPE_PRESSURE]->address;
    else
        return 0;
}

/**
 *  @brief   Get the ID of the pressure in use.
 *  @return  ID of the pressure in use.
 */
unsigned short inv_get_pressure_id(void)
{
    INVENSENSE_FUNC_START;
    struct mldl_cfg *mldl_cfg = inv_get_dl_config();
    if (mldl_cfg->slave[EXT_SLAVE_TYPE_PRESSURE]) {
        return mldl_cfg->slave[EXT_SLAVE_TYPE_PRESSURE]->id;
    }
    return ID_INVALID;
}

/**
 *  @brief  Get a sample of pressure data from the device.
 *  @param  data
 *              the buffer to store the pressure raw data for
 *              X, Y, and Z axes.
 *  @return INV_SUCCESS or a non-zero error code.
 */
inv_error_t inv_get_pressure_data(long *data)
{
    inv_error_t result = INV_SUCCESS;
    unsigned char tmp[3];
    struct mldl_cfg *mldl_cfg = inv_get_dl_config();

    /*--- read the pressure sensor data.
          The pressure read function may return an INV_ERROR_PRESSURE_* errors
          when the data is not ready (read/refresh frequency mismatch) or 
          the internal data sampling timing of the device was not respected. 
          Returning the error code will make the sensor fusion supervisor 
          ignore this pressure data sample. ---*/
    result = (inv_error_t) inv_mpu_read_pressure(mldl_cfg,
                                                 inv_get_serial_handle(),
                                                 inv_get_serial_handle(), tmp);
    if (result) {
        _pressureDebug(MPL_LOGV
                       ("inv_mpu_read_pressure returned %d (%s)\n", result,
                        MLErrorCode(result)));
        return result;
    }
    if (EXT_SLAVE_BIG_ENDIAN ==
        mldl_cfg->slave[EXT_SLAVE_TYPE_PRESSURE]->endian)
        data[0] =
            (((long)((signed char)tmp[0])) << 16) + (((long)tmp[1]) << 8) +
            ((long)tmp[2]);
    else
        data[0] =
            (((long)((signed char)tmp[2])) << 16) + (((long)tmp[1]) << 8) +
            ((long)tmp[0]);

    return INV_SUCCESS;
}

/**
 *  @}
 */
