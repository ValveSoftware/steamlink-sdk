/*
    Copyright (C) 2010 Rocky Bernstein <rocky@gnu.org>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
   \file mmc_hl_cmds.h 
   
   \brief Higher-level MMC commands which build on top of the lower-level
   MMC commands.
 */

#ifndef __CDIO_MMC_HL_CMDS_H__
#define __CDIO_MMC_HL_CMDS_H__

#include <cdio/mmc.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

  /**
     Close tray using a MMC START STOP UNIT command.
     @param p_cdio the CD object to be acted upon.
     @return DRIVER_OP_SUCCESS (0) if we got the status.
     return codes are the same as driver_return_code_t
  */
  driver_return_code_t mmc_close_tray( CdIo_t *p_cdio );
  
  /**
     Detects if a disc (CD or DVD) is erasable or not.
     
     @param p_cdio the CD object to be acted upon.
     
     @param b_erasable, if not NULL, on return will be set indicate whether
     the operation was a success (DRIVER_OP_SUCCESS) or if not to some
     other value.
     
     @return true if the disc is detected as erasable (rewritable), false
     otherwise.
  */
  driver_return_code_t mmc_get_disc_erasable(const CdIo_t *p_cdio, 
                                             bool *b_erasable);
    
  /**
    Eject using MMC commands. If CD-ROM is "locked" we'll unlock it.
    Command is not "immediate" -- we'll wait for the command to complete.
    For a more general (and lower-level) routine, @see mmc_start_stop_unit.

   @param p_cdio the CD object to be acted upon.
   @return DRIVER_OP_SUCCESS (0) if we got the status.
   return codes are the same as driver_return_code_t  
  */
  driver_return_code_t mmc_eject_media( const CdIo_t *p_cdio );
  
  /**
     Detects the disc type using the SCSI-MMC GET CONFIGURATION command.
     
     @param p_cdio the CD object to be acted upon.
   
     @param i_timeout_ms, number of millisections to wait before timeout
   
     @param p_disctype the disc type set on success.
     @return DRIVER_OP_SUCCESS (0) if we got the status.
   return codes are the same as driver_return_code_t
  */
  driver_return_code_t mmc_get_disctype(const CdIo_t *p_cdio, 
                                        unsigned int i_timeout_ms,
                                        cdio_mmc_feature_profile_t *p_disctype);

  /**
     Run a SCSI-MMC MODE_SENSE command (6- or 10-byte version) 
     and put the results in p_buf 
     @param p_cdio the CD object to be acted upon.

     @param p_buf pointer to location to store mode sense information

     @param i_size number of bytes allocated to p_buf

     @param page which "page" of the mode sense command we are interested in

     @return DRIVER_OP_SUCCESS if we ran the command ok.
  */
  driver_return_code_t mmc_mode_sense( CdIo_t *p_cdio, /*out*/ void *p_buf,
				       unsigned int i_size, int page);
  
  /**
    Set the drive speed in CD-ROM speed units.

    @param p_cdio          CD structure set by cdio_open().
    @param i_drive_speed   speed in CD-ROM speed units. Note this
                           not Kbs as would be used in the MMC spec or
                           in mmc_set_speed(). To convert CD-ROM speed units 
                           to Kbs, multiply the number by 176 (for raw data)
                           and by 150 (for filesystem data). On many CD-ROM 
                           drives, specifying a value too large will result 
                           in using the fastest speed.

    @return the drive speed if greater than 0. -1 if we had an error. is -2
    returned if this is not implemented for the current driver.

     @see cdio_set_speed and mmc_set_speed
  */
  driver_return_code_t mmc_set_drive_speed( const CdIo_t *p_cdio, 
                                            int i_drive_speed );


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CDIO_MMC_HL_CMDS_H__ */

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
