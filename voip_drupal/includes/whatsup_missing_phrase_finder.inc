<?php
/* $Id: whatsup_missing_phrase_finder.inc,v 1.3 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Basic functionality required for Asterisk users to use the What's Up System
 *
 */


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = "missing_phrase_finder.ini";
}

global  $agi;
if( !isset( $agi)) {
  $phpagi_config = parse_ini_file($configuration_file, TRUE);
  $phpagi_config = $phpagi_config['phpagi'];
  $phpagi_config_dir = isset($phpagi_config['phpagi_config_dir'])? $phpagi_config['phpagi_config_dir'] : "/etc/asterisk/phpagi.conf";
	
   require_once('phpagi.php');
   $agi = new AGI($phpagi_config_dir);
}

global $mpf_output_dir;
if (!isset($mpf_output_dir)) {
  $mpf_config = parse_ini_file($configuration_file, TRUE);
  $mpf_config = $mpf_config['whatsup_missing_phrase_finder'];
  $mpf_output_dir = isset($mpf_config['mpf_output_dir'])? $mpf_config['mpf_output_dir'] : "/tmp";
}

// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'voip_io.inc', 'error_handler.inc', 'voip_translate.inc', 'whatsup_voice_manager.inc') as $file) {
    require_once($file);
}


// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

/*
 * @return
 *   boolean TRUE indicates success.  If FALSE, check $eh_error_msg() for explanation.
 */
function mpf_main() {
eh_log("whatsup_missing_phrase_finder())");

  // start interacting with the caller
  vio_beep();
  vio_say("Welcome to the Missing Phrase Finder system.");
  
  $rc = TRUE;
  $main_menu = TRUE;
  while ($main_menu) {

    // main menu
    
    $voice_entry = wvm_select_voice(FALSE);
    if ($voice_entry == -1) {
      return FALSE;
    }
    else if ($voice_entry === 0) {
      continue;
    }
    
    // retrieve the missing recordings for the given voice
    $missing_recordings = vt_get_missing_phrase_recordings($voice_entry);
    if ($missing_recordings == -1) {
      $msg = 'Error retrieving missing phrase recordings.';
      eh_error($msg);
      vio_say($msg);
      return FALSE;
    }
    
    // now create a file with the entries retrieved
    global $mpf_output_dir;
    $phrase_file_name = $mpf_output_dir . "/missing_phrases-" . $voice_entry['language'] . '-' . $voice_entry['vid'] . '-' . date("Ymd-His") . '.tsv';
    $phrase_file = fopen($phrase_file_name, 'a');
    fputs($phrase_file, "phrase id\tphrase\tplaceholders\n");
    foreach($missing_recordings as $entry) {
      fputs($phrase_file, $entry['pid'] . "\t" . $entry['phrase'] . "\t" . $entry['placeholders']. "\n");
    }
    fclose($phrase_file);

    vio_say("Phrase file created with " . count($missing_recordings) . " phrases.");
    $main_menu = FALSE;
  }
  return TRUE;
}


?>
