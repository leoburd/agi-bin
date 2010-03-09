<?php
/* $Id: error_handler.inc,v 1.4 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Basic error handler functionality.
 *
 */


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'phpagi.php') as $file) {
   require_once($file);
}


// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------


global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = "error_handler.ini";
}
$eh_config = parse_ini_file($configuration_file, TRUE);
$eh_config = $eh_config['error_handler'];
  
global $eh_agi_debug_on;
if (!isset($eh_agi_debug_on)) {
  $eh_agi_debug_on = isset($eh_config['eh_agi_debug_on'])?$eh_config['eh_agi_debug_on']:FALSE;
}
  
global $eh_log_file;
if (!isset($eh_log_file)) {
  $eh_log_file = isset($eh_config['eh_log_file'])?$eh_config['eh_log_file']:'/var/log/error_handler.log';
}

// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

/*
 * Handles error messages.
 *
 * @param
 *   string error message
 * @return
 *   void
 */
function eh_error($message, $log = TRUE ) {

  global $eh_error;

  $eh_error = new stdClass();
  $eh_error->is_error = TRUE;
  $eh_error->message = $message;

  if ($log) {
    eh_log($message);
  }
}

/* 
 * Resets error handler
 *
 */
function eh_reset() {
  global $eh_error;

  $eh_error = new stdClass();
  $eh_error->is_error = FALSE;
}


/*
 * Returns the last error message.
 *
 * @return
 *   string error message, or FALSE in case of no error
 */
function eh_error_msg() {
  global $eh_error;

  return isset($eh_error->is_error) ? $eh_error->message : FALSE;
}


/*
 * Save the given line in the log file for debugging
 *
 * @param
 *   string text to be logged
 * @return
 *   void
 */
function eh_log($line) {
  global $eh_log_file;
  global $eh_agi_debug_on;

  $log_file = fopen($eh_log_file, 'a');
  $current_time = date("Y.m.d-H:i:s");
  fputs($log_file, "[$current_time] " . $line . "\n");
  fclose($log_file);

  if ($eh_agi_debug_on) {
  	 global $agi;
  	 if (!isset( $agi)) {
       $agi = new AGI();
     }
     $agi->conlog($line);
  }
}

// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------

?>
