<?php

/* $Id: asa_upload_entries_from_queue.php,v 1.4 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Uploads audioblog entries from the queue to their appropriate server
 *
 */

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = '/data/var/lib/asterisk/agi-bin/lawrence/whatsup.ini';  // full path to configuration file
}

$eh_agi_debug_on = FALSE;


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'audio_server_api.inc' ) as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}


// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  asa_upload_audio_entries_from_queue(3);

?>
