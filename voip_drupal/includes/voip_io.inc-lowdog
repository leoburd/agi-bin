<?php
/* $Id: voip_io.inc,v 1.6 2006/10/27 15:04:02 leob Exp $ */

/**
 * @file
 * Basic functionality required for VoIP input and output from callers
 *
 */


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------
foreach (array( 'error_handler.inc', 'phpagi.php', 'voip_translate.inc') as $file) {
   require_once($file);
}


// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------
global $vio_beep_file;
if (!isset($vio_beep_file)) {
  $vio_beep_file = 'beep';
}
  
// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------


/*
 * Read the given text outloud.
 *
 * @param $text
 *   string with the text to be read
 *
 * @param $escape_digits
 *   string with the keys that can be used to stop reading the text
 *
 * @return
 *   boolean TRUE in case of success.
 */
function vio_say_original($text, $escape_digits = '', $frequency = 8000) {

  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $agi->text2wav($text, $escape_digits, $frequency);

  return TRUE;
}

/*
 * Read the given text outloud.
 *
 * @param $text
 *   string or array with a combination of files and texts to be played/read to the user. 
 *   Note: file names start with '*'
 *
 * @param $escape_digits
 *   string with the keys that can be used to stop reading the text
 *
 * @return
 *   int -1 in case of failure, NULL in case the text is read until the end without interruptions, 
 *   or the key pressed in case any of the escape digits is typed while the text is being read
 */
function vio_say($text, $escape_digits = '', $frequency = 8000) {

  $moh_status = vio_stop_music_on_hold();
	
// TODO: this function should retrieve text-converted files (automatically generated or not) from appropriate language/locale directory
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $arguments = array();
  if (is_string($text)) {
    $arguments[] = $text;
  }
  else {
    $arguments = $text;
  }

  // TODO: For automatic text-to-speech generation, this function should concatenate as many string elements as possible before doing the conversion.
  $optimized_element = '';
  $optimized_array = array();
  while (!is_null($element = array_shift($arguments))) {
  	if (!$element) {
  	  continue;
  	}
    if ($element{0} != '*') {  
      $optimized_element .= $element;
    }
    else {
      $optimized_array[] = $optimized_element;
      $optimized_array[] = $element;
      $optimized_element = '';
    }
  }
  if ($optimized_element) {
    $optimized_array[] = $optimized_element;
  }
  $arguments = $optimized_array;

  // now read/play the text  
  $rc = NULL;
  foreach ($arguments as $prompt) {
  	if (!$prompt) {
  	  continue;
  	}
    if($prompt{0} == '*') {
      $ret = $agi->stream_file(substr($prompt, 1), $escape_digits);
    }
    else {
      $ret = $agi->text2wav($prompt, $escape_digits, $frequency);
    }
    if ($ret === TRUE) { // NOTE: added on 2005.12.21 to compensate for $prompt == " ";
      continue;
    }
    if ($ret['code'] != AGIRES_OK || $ret['result'] == -1) {
      $rc = -1;
      break;
    }
    if ($ret['result'] != 0) {
      $rc = chr($ret['result']);
      break;
    }
  }
  if ($moh_status) {
    vio_start_music_on_hold();  
  }
  return $rc;
}

/*
 * Present the user with a menu and read the response.
 *
 * @param $choices
 *   array the menu choices.  The index of the array -- which has to be a single digit -- indicates 
 *   the option to be pressed by the user.  The actual content of the element is either a string 
 *   or an array of strings.  If the string (or strings) starts with '*', then it indicates the 
 *   name of the audio file to be played.  If it does not start with a '*', then it will be read out loud. 
 *
 * @param $timeout
 *   mixed. Maximum time to wait for a user response.
 * 
 * @param $max_tries
 *   int. Number maximum of times the menu will be presented until a valid option is selected
 *
 * @return
 *   mixed: key pressed on sucess, -1 on failure, NULL if no valid choice is pressed within the max number of attempts allowed
 */
function vio_menu($choices, $max_tries = 0, $timeout = 2000) {
	
  $moh_status = vio_stop_music_on_hold();
  
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $keys = join('', array_keys($choices));

  $choice = NULL;
  $counter = 0;
  $running = TRUE;
  while ($running) {
    foreach($choices as $prompt) {
      $choice = vio_say($prompt, $keys);
      if ($choice) {
        break;
      }
    }
    if (is_null($choice)) {
      global $vio_beep_file;
      $ret = $agi->get_data($vio_beep_file, $timeout, 1);
      if ($ret['code'] != AGIRES_OK || $ret['result'] == -1) {
      	$running = FALSE;
        $choice = -1;
      }
      else {
      	if ($ret['result'] != '' && strpos(' '.$keys, $ret['result'])) {
      	  $running = FALSE;
          $choice = $ret['result'];
      	}
      	else {
      	  $counter++;
      	  if ($max_tries && ($counter >= $max_tries)) {
      	    $running = FALSE;
      	  }
      	}
      }
    }
    else {
      $running = FALSE;
    }
  }

  if ($moh_status) {
    vio_start_music_on_hold();
  }
  
  return $choice;
}

function vio_menu_old($choices, $timeout = 2000) {
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }
  $rc = $agi->menu($choices, $timeout);
  return $rc;
}


/*
 * Make a beep
 *
 */
function vio_beep() {
  global $vio_beep_file;
  
  $moh_status = vio_stop_music_on_hold();
  
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }
  $agi->stream_file($vio_beep_file);
  
  if ($moh_status) {
  	vio_start_music_on_hold();
  }
}


/*
 * Let caller control (FFD, RWD, etc.) the way the specified file is played.
 *
 * @param $basename
 *   filename (without the suffix) of the file to be played
 *
 * @param $escape_digits
 *   string with the keys that can be used to stop playing the file
 *
 * @return
 *   FALSE in case of error.  array with ['success'] = TRUE and ['key_pressed']s
 *   indicating the key that the user pressed to stop the audio narration.  If
 *   'key_pressed' == NULL, the reading got to its end.
 */
function vio_control_stream_file( $basename, $escape_digits = '') {

  $moh_status = vio_stop_music_on_hold();
  
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $offset = '0';		// in millisecs
  $ffchar = '3';                // forward key
  $rewchar = 1;                 // rewind char

  $rc = array();
  $control_digits = $escape_digits . " $ffchar $rewchar";
  $stop_playing = FALSE;
  $pause = FALSE;

  while (!$stop_playing) {

    // todo: Someday we may replace stream file by control stream file and simplify the function below...
    //  $r = $agi->evaluate("CONTROL STREAM FILE $basename \"$escape_digits\" $offset $ffchar $rewchar $pausechar");
    $r = $agi->stream_file($basename, $control_digits, $offset);
    if ($r['result'] == -1) {
      eh_error("Error streaming file: $basename.");
      if ($moh_status) {
  	    vio_start_music_on_hold();
      }
      return FALSE;
    }
    else {
      if ($r['endpos'] == 0) {
        eh_error("Error opening file: $basename . Are you sure the file exists? Is it really a .wav or Asterisk-compatible file?");
        if ($moh_status) {
  	      vio_start_music_on_hold();
        }
        return FALSE;
      }

      if ($r['result'] == 0) {
        // if file played until the end...
        $rc['success'] = TRUE;
        $rc['key_pressed'] = NULL;
        $stop_playing = TRUE;
      }
      else {
        // handle control keys
        $key = chr($r['result']);
        if (strpos($escape_digits, $key) !== FALSE) {
          $rc['success'] = TRUE;
          $rc['key_pressed'] = $key;
          $stop_playing = TRUE;
        }
        else {

          switch ($key) {
            case $ffchar:
              $offset = $r['endpos'] + 100000;	// advance 10 second
              break;

            case $rewchar:
              $offset = ($r['endpos'] <= 50000) ? 0 : $r['endpos'] - 50000;	// rewind 5 second
              break;

            default:
              eh_error("Invalid key detected");
              $rc = FALSE;
              $stop_playing = TRUE;
          }
        }
      }
    }
  }
  if ($moh_status) {
  	vio_start_music_on_hold();
  }
  return $rc;
}




/*
 * Use Festival to read text and get user input.
 *
 * @param $prompt
 *   string )(generated by either t() or v()) with either the filename or the text to be read to the caller asking her to input data. 
 * 
 * @param $max_tries
 *   int maximum number of times the prompt should be repeated until the function returns an empty value.
 * 
 * @param $no_input_msg
 *   string message to be played in case the user does not type anything within the specified time.
 * 
 * @param $timeout
 *   int maximum number of millisecond that the program waits for user input before returning.  Default: 2000ms
 *
 * @param $max_digits
 *   maximum number of digits expected.  If NULL, then there is no limit
 *
 * @return
 *   -1 in case of error, or string with the input data.
 * 
 */
function vio_get_input($prompt, $max_tries=1, $no_input_msg='', $timeout=NULL, $max_digits=NULL, $clean_cache = TRUE) {

  $moh_status = vio_stop_music_on_hold();
  
  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $frequency = 8000;
  $timeout = (!$timeout)?2000:$timeout;
  
  // in case prompt has been generated by v(), use only the first element of the speech array as the prompt
  // TODO: create a function that "flattens" a received speech array into a single audio file... 
  if (is_array($prompt)) {
    $prompt = $prompt[0];
  }

  $prompt = trim($prompt);
  if ($prompt == '') {
    if ($moh_status) {
  	  vio_start_music_on_hold();
    }
eh_log('error: prompt is empty!!');
    return -1;
  }
  else if($prompt{0} == '*') {
    $fname = substr($prompt, 1);
  }
  else {

    $hash = md5($prompt);
    $fname = $agi->config['phpagi']['tempdir'] . DIRECTORY_SEPARATOR;
    $fname .= 'get_input_' . $hash;

    // create the .wav file
    if(!file_exists("$fname.wav"))
    {
      // write text file
      if (!file_exists("$fname.txt")) {
        $fp = fopen("$fname.txt", 'w');
        fputs($fp, $prompt);
        fclose($fp);
      }

      shell_exec("{$agi->config['festival']['text2wave']} -F $frequency -o $fname.wav $fname.txt");
    }
    else {
      touch("$fname.txt");
      touch("$fname.wav");
    }
  }
  
  // play file and get user input
  $tries = 0;
  $stop = FALSE;
  while(($tries < $max_tries) && !$stop) {
    $ret = $agi->get_data($fname, $timeout, $max_digits);
    if (($ret['code'] != AGIRES_OK) || ($ret['result'] == -1)) {
      $rc = -1;
      $stop = TRUE;
    }
    // NOTE: fixed a bug here that prevented users from typing 0...
    else if (($ret['result']) || ($ret['result'] === '0')) {
      $rc = $ret['result'];
      $stop = TRUE;
    }
    else { // no input received
       if (!empty($no_input_msg)) {
         vio_say($no_input_msg);
       }
       $tries++;
    }
  }
  
  if ($clean_cache) {
    // clean up cache files that haven't been accessed in over a month
    $delete = time() - 2592000; // 1 month
    foreach (glob($agi->config['phpagi']['tempdir'] . DIRECTORY_SEPARATOR . 'get_input_*') as $file) {
     if (filemtime($file) < $delete) {
        unlink($file);
      }
    }
  }
  
  if ($moh_status) {
    vio_start_music_on_hold();
  }

  if (($ret['code'] != AGIRES_OK) || ($ret['result'] == -1)) {
eh_log('error: result is -1!!');
    return -1;
  }
  else {
    return $ret['result'];
  }
}

function vio_get_text($prompt, $max_digits=NULL, $timeout=NULL, $invalid_character_prompt = NULL) {

  $moh_status = vio_stop_music_on_hold();

  static $key2digit = array (
    '2' => 'a', '22' => 'b', '222' => 'c', '2222' => '2',
    '3' => 'd', '33' => 'e', '333' => 'f', '3333' => '3',
    '4' => 'g', '44' => 'h', '444' => 'i', '4444' => '4',
    '5' => 'j', '55' => 'k', '555' => 'l', '5555' => '5',
    '6' => 'm', '66' => 'n', '666' => 'o', '6666' => '5',
    '7' => 'p', '77' => 'q', '777' => 'r', '7777' => 's', '77777' => '7',
    '8' => 't', '88' => 'u', '888' => 'v', '8888' => '8',
    '9' => 'w', '99' => 'x', '999' => 'y', '9999' => 'z', '99999' => '9',
    '0' => 0, '*' => ' ',
  );

  if (!$invalid_character_prompt) {
    $invalid_character_prompt = v('Invalid character. Please try again.');
  }
  
  $text = '';
  $c_digits = 0;
  $stop = FALSE;
  $prompt_tmp = $prompt;
  while (!$stop) {
    $input = vio_get_input($prompt_tmp, 1, '', $timeout, 5, FALSE);
eh_log("input: $input, text: $text");
    if ($input == -1) {
      if (!$moh_status) {
        vio_start_music_on_hold();
      }
      return -1;
    }
    else if (($input == '') || ($input == '#')) {
      $stop = TRUE;
    }
    else if (isset($key2digit[$input])) {
      $prompt_tmp = v(_vio_digit2sound($key2digit[$input]));
      $text .= $key2digit[$input];
      $c_digits++;
      if ((!is_null($max_digits)) && ($c_digits >= $max_digits)) {
      	// play the last character typed in
      	vio_say($prompt_tmp);
        $stop = TRUE;
      }
    }
    else {
      $prompt_tmp = $invalid_character_prompt;
    }
  }
  
  if ($moh_status) {
    vio_start_music_on_hold();
  }

  if (!$stop) {
    return '';
  }
  
  return $text;

}

function _vio_digit2sound($digit) {
  static $digit2sound = array (
    0 => '0', 1 => '1', 2 => '2', 3 => '3', 4 => '4', 
    5 => '5', 6 => '6', 7 => '7', 8 => '8', 9 => '9', 
    'a' => 'ay', 'b' => 'bee', 'c' => 'cee',
    'd' => 'D.', 'e' => 'eee', 'f' => 'f',
    'g' => 'G.', 'h' => 'h', 'i' => 'i',
    'j' => 'jay', 'k' => 'kay', 'l' => 'l',
    'm' => 'M.', 'n' => 'n', 'o' => 'o', 
    'p' => 'p', 'q' => 'q', 'r' => 'r', 's' => 's',
    't' => 'tee', 'u' => 'u', 'v' => 'v',
    'w' => 'w', 'x' => 'x', 'y' => 'y', 'z' => 'z',
    ' ' => 'space',
  );
  
  return isset($digit2sound[$digit])?$digit2sound[$digit]:'';
}

function vio_say_digits($string) {
  $moh_status = vio_stop_music_on_hold();

  $count = strlen($string);
  for ($i = 0; $i < $count; $i++) {
    vio_say(v(_vio_digit2sound($string{$i})));
  }
  
  if ($moh_status) {
    vio_start_music_on_hold();
  }
}

/*
 * Set the class to be used by music on hold
 *
 * @param string $class
 * @return string the current music on hold class
 */
function vio_set_music_on_hold_class($new_class=NULL) {

  static $class = '';
  if (!is_null($new_class)) {
    $class = $new_class;
  }
  return $class;
}

/*
 * Start playing music on hold to the caller.
 *
 * @param string $class
 * @return void
 */
function vio_start_music_on_hold() {

  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  if (! _vio_is_moh_active()) {
  	$class = vio_set_music_on_hold_class();
    $agi->exec_start_musiconhold($class);
    _vio_is_moh_active(TRUE);
  }
}

/*
 * Stop playing music on hold to the caller.
 *
 * @return boolean the status of music on hold right before the call
 */
function vio_stop_music_on_hold() {

  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  if ($status = _vio_is_moh_active()) {
    $agi->exec_stop_musiconhold();
    _vio_is_moh_active(FALSE);
  }
  
  return $status;
}


/*
 * Return a .wav file with audio recorded from the user.
 *
 * @param mixed $prompt to be read to caller before doing the recording
 * @param string $escape_digits
 * @param integer $time_limit is the maximum record time in second, or -1 for no limit.
 * @param boolean $beep
 * @param integer $silence number of second of silence allowed before the function returns despite the
 * lack of dtmf digits or reaching timeout.
 * @return string, name of the file recorded (with the .wav suffix) or -1 in case of error
 */
function vio_record_audio($prompt='', $escape_digits='#', $time_limit=-1, $beep=TRUE, $silence=NULL) {

  $moh_status = vio_stop_music_on_hold();

  global  $agi;
  if (!isset($agi)) {
    $agi = new AGI();
  }

  $tempdir = $agi->config['phpagi']['tempdir'];
  $file = tempnam($tempdir, 'vio_');
  rename($file, $file . '.wav'); 

  // convert time limit to second
  $time_limit = ($time_limit == -1) ? $time_limit : $time_limit * 1000;

  $choices = array();
  $choices[1] = "*To review this recording, press 1.";
  $choices[2] = "*To accept it, press 2.";
  $choices[3] = "*To redo the recording, press 3.";

  $stop = FALSE;
  while (!$stop) {

    vio_say($prompt);    
    $ret = $agi->record_file($file, "wav", $escape_digits, $time_limit, NULL, $beep, $silence);
    if (($ret['code'] != AGIRES_OK) || ($ret['result'] == -1)) {
      unlink($file);
      if ($moh_status) {
        vio_start_music_on_hold();  
      }
      return -1;
    }

    $menu_choices = TRUE;
    while ($menu_choices) {
      $choice = $agi->menu($choices);

      switch ($choice) {
       case 1:
         $agi->stream_file('beep');
         $r = $agi->stream_file($file, '#');
         break;

        case 2:
          // add the suffix .wav to the filename
          $file = $file . '.wav';
          if ($moh_status) {
            vio_start_music_on_hold();  
          }
          return $file;

       case 3:
         $menu_choices = FALSE;
         break;

       default: // processing error
         unlink($file);
         if ($moh_status) {
           vio_start_music_on_hold();  
         }
         return -1;
      }
    }
  }
}

/*
 * Get date input from caller
 * 
 * @input $format 
 * string with any combination of the following:
 *   d : day with leading zeroes
 *   m : month with leading zeroes
 *   Y : year, 4 digits
 * 
 * @return
 * FALSE in case of error. In case of success, return array with 'day', 'month' and 'year'
 */
function vio_get_date($prompt, $invalid_date_msg = '', $format = 'mdY') {
  
  $moh_status = vio_stop_music_on_hold();
	
  // calculate minimum and max number of digits to be input
  $max = strlen($format);
  $date = array('year' => NULL, 'month' => NULL, 'day' => NULL);
  $max_digits = 0;
  $min_digits = 0;
  for ($i = 0; $i < $max; $i++) {
    $c = $format[$i];
    if (strpos('dm', $c) !== FALSE) {
      $max_digits += 2;
      $min_digits += 2;
    }
    else if ($c == 'Y') {
      $max_digits += 4;
      $min_digits += 4;
    }
  }
  // read the input from the caller
  $tries = 0;
  $max_tries = 3;
  $stop = FALSE;
  while(($tries < $max_tries) && !$stop) {
    $invalid_input = FALSE;
    $input = vio_get_input($prompt, 1, NULL, NULL, $max_digits);
    switch ($input) {
      case -1:
        eh_error('Error reading input from the user.');
        if ($moh_status) {
          vio_start_music_on_hold();
        }
	    return FALSE;
      case '': // user didn't type anything.
        eh_log('User did not press any key.');
        $invalid_input = TRUE;
        break;
      default:    
        // check if number of digits input is valid
        if (strlen($input) < $min_digits) {
          $invalid_input = TRUE;
          break;
        }
        // check if input is numeric
        if(!is_numeric($input)) {
          $invalid_input = TRUE;
          break;   
        }    	
        // parse and validate the input
        $input_index = 0;
        for ($i = 0; (($i < $max) && (!$invalid_input)); $i++) {
          $c = $format[$i];
          switch ($c) {
            case 'd':
              $day = (int) substr($input, $input_index, 2);
              if (($day <= 0) || ($day > 31)) {
                $invalid_input = TRUE;
              }
              else {
                $date['day'] = $day;
                $input_index += 2;
              }
              break;
            case 'm':
              $month = (int) substr($input, $input_index, 2);
              if (($month <= 0) || ($month > 12)) {
                $invalid_input = TRUE;
              }
              else {
                $date['month'] = $month;
                $input_index += 2;
              }
              break;
            case 'Y':
              $year = (int) substr($input, $input_index, 4);
              $date['year'] = $year;
              $input_index += 4;
              break;
          } 
        } // for
        // now validate months vs. days
        if (!$invalid_input) {
          if (isset($date['month']) && isset($date['day'])) {
            static $short_months = array(4, 6, 9, 11);
            if (in_array($date['month'], $short_months) && ($date['day'] > 30)) {
              $invalid_input = TRUE;
            } else if (($date['month'] == 2) && ($date['day'] > 28)) {
           	  if ($date['day'] > 29) {
           	    $invalid_input = TRUE;
           	  }
           	  else if (isset($date['year'])){
           	    $y = $date['year'];
                $is_leap_year = ($y % 4 == 0 && ($y % 400 == 0 || $y % 100 != 0)); 
                if (!$is_leap_year) {
                  $invalid_input = TRUE;
                }
              }
            }
          }
        }
    } // switch    
    if (!$invalid_input) {
      $rc = $date;
      $stop = TRUE;
    }
    else {
      if (!empty($invalid_date_msg)) {
        vio_say($invalid_date_msg);
      }
      $rc = FALSE;
      $tries++;
    }
  } // while
  if (!$moh_status) {
    vio_start_music_on_hold();
  }
  return $rc;
}

/*
 * Get time input from caller
 * 
 * @input $format 
 * string with any combination of the following:
 *   h : 12-hour format of an hour with leading zeros
 *   H : 24-hour format of an hour with leading zeros
 *   i : minutes with leading zeroes
 *   s : seconds with leading zeroes
 * 
 * @return
 * FALSE in case of error. In case of success, return array with 'hour' (00 through 23), 'minute' and 'second'
 */
function vio_get_time($prompt, $invalid_time_msg = '', $format = 'hi', $is_am_msg = NULL, $is_pm_msg = NULL) {
  $moh_status = vio_stop_music_on_hold();
  
  $is_am_msg = (isset($is_am_msg))?$is_am_msg:'';
  $is_pm_msg = (isset($is_pm_msg))?$is_pm_msg:'';
  
  // calculate minimum and max number of digits to be input
  $max = strlen($format);
  $time = array('hour' => NULL, 'minute' => NULL, 'second' => NULL);
  $max_digits = 0;
  $min_digits = 0;
  for ($i = 0; $i < $max; $i++) {
    $c = $format[$i];
    if (strpos('hHis', $c) !== FALSE) {
      $max_digits += 2;
      $min_digits += 2;
    }
  }
  // read the input from the caller
  $tries = 0;
  $max_tries = 3;
  $stop = FALSE;
  while(($tries < $max_tries) && !$stop) {
    $invalid_input = FALSE;
    $input = vio_get_input($prompt, 1, NULL, NULL, $max_digits);
    switch ($input) {
      case -1:
        eh_error('Error reading input from the user.');
        if ($moh_status) {
          vio_start_music_on_hold();
        }
	    return FALSE;
      case '': // user didn't type anything.
        eh_log('User did not press any key.');
        $invalid_input = TRUE;
        break;
      default:    
        // check if number of digits input is valid
        if (strlen($input) < $min_digits) {
          $invalid_input = TRUE;
          break;
        }
        // check if input is numeric
        if(!is_numeric($input)) {
          $invalid_input = TRUE;
          break;   
        }    	
        // parse and validate the input
        $confirm_ampm = FALSE;
        $input_index = 0;
        for ($i = 0; (($i < $max) && (!$invalid_input)); $i++) {
          $c = $format[$i];
          switch ($c) {
            case 'h':
              $hour = (int) substr($input, $input_index, 2);
              if (($hour > 12)) {
                $invalid_input = TRUE;
              }
              else {
                $time['hour'] = $hour;
                $confirm_ampm = TRUE;
                $input_index += 2;
              }
              break;
            case 'H':
              $hour = (int) substr($input, $input_index, 2);
              if (($hour > 24)) {
                $invalid_input = TRUE;
              }
              else {
                $time['hour'] = $hour;
                $confirm_ampm = FALSE;
                $input_index += 2;
              }
              break;
            case 'i':
              $minute = (int) substr($input, $input_index, 2);
              if ($minute > 59) {
                $invalid_input = TRUE;
              }
              else {
                $time['minute'] = $minute;
                $input_index += 2;
              }
              break;
            case 's':
              $second = (int) substr($input, $input_index, 2);
              if ($second > 59) {
                $invalid_input = TRUE;
              }
              else {
                $time['second'] = $second;
                $input_index += 2;
              }
              break;
           } 
        } // for
    } // switch    
    if (!$invalid_input) {
      // now check if time is AM or PM
      if ($confirm_ampm) {
      	$choices = array();
      	$choices[1] = $is_am_msg;
      	$choices[3] = $is_pm_msg;
        $choice = vio_menu($choices);
        switch($choice) {
          default: // invalid option
          case -1: // processing error
            eh_error("Error selecting AM/PM options (choice: $choice)");
            if ($moh_status) {
              vio_start_music_on_hold();
            }
            return FALSE;
          case 1:
            // process the midnight case where 12:00am is in fact 24:00
            $time['hour'] = ($time['hour'] == 12)? ($time['hour'] + 12) : $time['hour'];
            break;
          case 3:
            $time['hour'] += 12;
            break;
        }
      }
      // all set
      $rc = $time;
      $stop = TRUE;
    }
    // invalid input
    else {
      if (!empty($invalid_time_msg)) {
        vio_say($invalid_time_msg);
      }
      $rc = FALSE;
      $tries++;
    }
  } // while
  if ($moh_status) {
    vio_start_music_on_hold();
  }
  return $rc;
}


// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------

function _vio_is_moh_active($new_status = NULL) {
  static $moh_active = FALSE;
  if (! is_null($new_status)) {
  	$moh_active = $new_status;
  }
  return $moh_active;
}

?>
