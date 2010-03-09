<?php

/* $Id: asa_cache_recycle.php,v 1.4 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Remove old files from the audioblog download cache
 *
 */


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

$configuration_file = 'whatsup.ini';

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

	asa_cache_recycle(10); // recycle files that are more than 10 days old

?>
