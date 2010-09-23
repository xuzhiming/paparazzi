/*
 * $Id$
 *  
 * Copyright (C) 2008-2009 Antoine Drouin <poinix@gmail.com>
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

#include <inttypes.h>

#include "std.h"
#include "init_hw.h"
#include "sys_time.h"
#include "led.h"
#include "uart.h"
#include "messages.h"
#include "downlink.h"

#include "i2c.h"
#include "booz/peripherals/booz_ami601.h"
#include "math/pprz_algebra_int.h"

#include "interrupt_hw.h"
#include "std.h"

static inline void main_init( void );
static inline void main_periodic_task( void );
static inline void main_event_task( void );

static inline void on_mag(void);

int main( void ) {
  main_init();
  while(1) {
    if (sys_time_periodic())
      main_periodic_task();
    main_event_task();
  }
  return 0;
}

static inline void main_init( void ) {
  hw_init();
  sys_time_init();

  LED_ON(4);
  ami601_init();

  int_enable();
}

static inline void main_periodic_task( void ) {
  //  RunOnceEvery(100, {DOWNLINK_SEND_ALIVE(DefaultChannel, 16, MD5SUM);});

  RunOnceEvery(10, { ami601_read();});
}

static inline void main_event_task( void ) {

  AMI601Event(on_mag);

}

static inline void on_mag(void) {
  LED_TOGGLE(4); 
  ami601_status = AMI601_IDLE;
  struct Int32Vect3 bla = {ami601_values[0], ami601_values[1], ami601_values[2]};
  DOWNLINK_SEND_IMU_MAG_RAW(DefaultChannel, &bla.x, &bla.y, &bla.z);
  
}