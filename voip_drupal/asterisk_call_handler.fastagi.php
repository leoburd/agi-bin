<?php
/* $Id$ */

/**
 * @file
 * PHP FastAGI script that runs the Voip Drupal system
 *
 * Note: Fast PHPAGI scripts should never start with #!/usr/bin/php ... they run as CGI PHP
 *     : Make sure you use PHP 5 for best performance
 */


// Usage:
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

/***********************

global  $agi;
if( !isset( $agi)) {
  global $configuration_file;
  if (!isset($configuration_file)) {
    $configuration_file = "phpagi.ini";
  }
  $phpagi_config = parse_ini_file($configuration_file, TRUE);
  $phpagi_config = $phpagi_config['phpagi'];
  $phpagi_config_file = isset($phpagi_config['phpagi_config_file'])? $phpagi_config['phpagi_config_file'] : "/etc/asterisk/phpagi.conf";
	
   require_once('includes'. DIRECTORY_SEPARATOR . 'phpagi.php');
   $agi = new AGI($phpagi_config_file);
}

***************/
// load local configuration

global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'whatsup.ini';  // full path to configuration file
}

$voip_config = parse_ini_file($configuration_file, TRUE);
$voip_config = $voip_config['voip'];


// define global $agi based on fastagi defined in the phpagi-fastagi.php file called by Asterisk

global $fastagi;
global  $agi;
if( !isset( $agi)) {
   $agi = $fastagi;
}

$fastagi->verbose("voip configuration file: $configuration_file");
$fastagi->verbose("configuration contents: " . print_r($voip_config, true));

$fastagi->verbose("This is fastagi: " . print_r($fastagi, true));
$fastagi->verbose("This is agi: " . print_r($agi, true));

// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

// include Voip Drupal-specific files

foreach (array( 'error_handler.inc', 'whatsup_client.inc') as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}
eh_log("(whatsup config) file: $configuration_file, config: " . print_r( parse_ini_file($configuration_file, TRUE), TRUE));

// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  // Retrieve mandatory variables associated with the server

  $voip_server = $voip_config['voip_server'];
// $voip_server = $voip_server . '?XDEBUG_SESSION_START=whatsup';

  // run the What's Up system

eh_log("--------------------------------");
eh_log("About to call:  $r = whatsup_main($voip_server);");
  $r = whatsup_main($voip_server);
eh_log("--------------------------------");

  if ($r) {	// in case of success...
  
    // set the return value to 1
    $agi->set_variable( "AB_RESULT", 1);

  } else {	// in case of failure...

    // set the return value to 0
    $agi->set_variable( "AB_RESULT", 0);
    $agi->set_variable( "AB_ERROR_MSG", eh_error_msg());
  }

  exit;  

?>
