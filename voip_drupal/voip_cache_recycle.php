<?php

/* $Id: voip_cache_recycle.php,v 1.1 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Remove voip extensions that have not been used in the specified time frame
 */


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

//TODO: how to pass the configuration file name via parameter?
$configuration_file = "whatsup.ini";
$voip_config = parse_ini_file($configuration_file, TRUE);
$voip_config = $voip_config['voip'];


$eh_agi_debug_on = FALSE;


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'voip_client.inc' ) as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}

// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  $voip_server = $voip_config['voip_server'];
  $user_info = array('user_name' => $voip_config['voip_recycle_username'], 'password' => $voip_config['voip_recycle_password']);
  voip_recycle_extensions($voip_server, $user_info, 1, 10);

?>
