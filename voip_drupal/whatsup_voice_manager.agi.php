#!/usr/bin/php -q
<?php
/* $Id: whatsup_voice_manager.agi.php,v 1.3 2006/11/01 12:03:22 leob Exp $ */
// #!/usr/bin/php5/bin/php

/**
 * @file
 * PHP AGI script that runs the voice manager for the What's Up system
 *
 * Note: PHPAGI scripts only run on CLI PHP... They do not work on CGI PHP.
 *     : Make sure you use PHP 5 for best performance
 */


// Usage:
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = "whatsup.ini";
}

global  $agi;
if( !isset( $agi)) {
  global $configuration_file;
  if (!isset($configuration_file)) {
    $configuration_file = "phpagi.ini";
  }
  $phpagi_config = parse_ini_file($configuration_file, TRUE);
  $phpagi_config = $phpagi_config['phpagi'];
  $phpagi_config_dir = isset($phpagi_config['phpagi_config_dir'])? $phpagi_config['phpagi_config_dir'] : "/etc/asterisk/phpagi.conf";
	
   require_once('includes'. DIRECTORY_SEPARATOR . 'phpagi.php');
   $agi = new AGI($phpagi_config_dir);
}



// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'whatsup_voice_manager.inc') as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}

// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  // Retrieve mandatory variables associated with the server

  // run the What's Up Voice Manager system

  $r = wvm_main();

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
