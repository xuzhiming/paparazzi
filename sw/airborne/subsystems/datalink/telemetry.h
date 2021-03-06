/*
 * Copyright (C) 2013 Gautier Hattenberger
 *
 * This file is part of paparazzi.
 *
 * paparazzi is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * paparazzi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with paparazzi; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

/**
 * @file subsystems/datalink/telemetry.h
 *
 * Periodic telemetry system header (includes downlink utility and generated code).
 *
 * In order to use it a subsystem/module:
 * - include this header:
 *
 *    #include "susystems/datalink/telemetry.h"
 *
 * - write a callback function:
 *
 *    void your_callback(void) {
 *      // your code to send a telemetry message goes here
 *    }
 *
 * - register your callback function (if the message name doesn't match
 *   one of the names in your telemetry xml file or is already registered,
 *   the function return FALSE)
 *
 *    register_periodic_telemetry(&your_telemetry_struct, "YOUR_MESSAGE_NAME", your_callback);
 *
 * In most cases, the default telemetry structure should be used
 * (replace &your_telemetry_struct by DefaultPeriodic in the register function).
 */

#include "std.h"
#include "messages.h"
#include "mcu_periph/uart.h"
#include "subsystems/datalink/downlink.h"
#include "generated/periodic_telemetry.h"

#endif /* TELEMETRY_H */
