<?php
/* $Id$ */

/**
 * @file
 * PHP FastAGI script that runs the Voip Drupal system
 *
 * Note: Fast PHPAGI scripts should never start with #!/usr/bin/php ...
 *       they run as CGI PHP
 *     : Make sure you use PHP 5 for best performance
 */


// -----------------------------------------------------------------------------
// initialize global variables
// -----------------------------------------------------------------------------

// load local configuration

global $configuration_file;
if (!isset($configuration_file)) {
  // TODO: replace whatsup.ini by voip_drupal.ini
  $configuration_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . 'whatsup.ini';
}

$voip_config = parse_ini_file($configuration_file, TRUE);
$voip_config = $voip_config['voip'];


// define global $agi based on the $fastagi variable defined in the
// phpagi-fastagi.php file called by Asterisk

global $fastagi;
global  $agi;
if( !isset( $agi)) {
   $agi = $fastagi;
}

$fastagi->verbose("voip configuration file: $configuration_file");
$fastagi->verbose("configuration contents: " . print_r($voip_config, true));

// $fastagi->verbose("This is fastagi: " . print_r($fastagi, true));
// $fastagi->verbose("This is agi: " . print_r($agi, true));

// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

// include Voip Drupal-specific files

foreach (array( 'error_handler.inc', 'asterisk_script_handler.inc') as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}
eh_log("(whatsup config) file: $configuration_file, config: " . print_r( parse_ini_file($configuration_file, TRUE), TRUE));

// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  // Retrieve mandatory variables associated with the server

  $voip_server = $voip_config['voip_server'];

  // read script arguments

  $vd_cid = $agi->request['agi_callerid']; 
  $vd_cid_name = $agi->request['agi_calleridname']; 
  $vd_xmlrpc_url = $agi->request['agi_arg_1']; 
  $vd_script_name = $agi->request['agi_arg_2']; 
  $vd_user_name =  $agi->request['agi_arg_3']; 

eh_log('Input arguments:');
eh_log('vd_cid: ' . $vd_cid);
eh_log('vd_cid_name: ' . $vd_cid_name);
eh_log('vd_xmlrpc_url: ' . $vd_xmlrpc_url);
eh_log('vd_script_name: ' . $vd_script_name);
eh_log('vd_user_name: ' . $vd_user_name);

  // run the Voip Drupal script handler

eh_log("--------------------------------");
eh_log('About to call Voip Drupal script handler');
  $options['cid_number'] = $vd_cid;
  $options['user_name'] = $vd_user_name;
  $r = asterisk_script_handler($vd_xmlrpc_url,$vd_script_name,$options);
eh_log("--------------------------------");

  // set return variables
  if ($r) {	// in case of success...
    $agi->set_variable("VD_RESULT",1);
    $agi->set_variable("VD_ERROR_MSG",'everything is alright');
  } else {      // in case of failure...

    // set the return value to 0
    $agi->set_variable("VD_RESULT", 0);
    $agi->set_variable("VD_ERROR_MSG",eh_error_msg());
  }
  
  exit;  

?>
