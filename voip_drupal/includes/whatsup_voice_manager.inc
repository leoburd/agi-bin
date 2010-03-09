<?php
/* $Id: whatsup_voice_manager.inc,v 1.4 2006/11/01 12:03:22 leob Exp $ */

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
  $configuration_file = "whatsup.ini";
}

global  $agi;
if( !isset( $agi)) {
  $phpagi_config = parse_ini_file($configuration_file, TRUE);
  $phpagi_config = $phpagi_config['phpagi'];
  $phpagi_config_dir = isset($phpagi_config['phpagi_config_dir'])? $phpagi_config['phpagi_config_dir'] : "/etc/asterisk/phpagi.conf";
	
   require_once('phpagi.php');
   $agi = new AGI($phpagi_config_dir);
}



// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'voip_io.inc', 'error_handler.inc', 'voip_translate.inc') as $file) {
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
function wvm_main() {
eh_log("whatsup_voice_manager())");

  // start interacting with the caller
  vio_beep();
  vio_say("Welcome to the Voice Manager system.");
  
  $rc = TRUE;
  $main_menu = TRUE;
  while ($main_menu) {

    // main menu
    
    $voice_entry = wvm_select_voice(TRUE);
    if ($voice_entry == -1) {
      vio_say(t('Processing error. Please contact your system administrator.'));
      return FALSE;
    }
    else if ($voice_entry === 0) {
      continue;
    }
    
    // now perform the recording/translation
    $recording = TRUE;
    while ($recording) {
      vio_beep();
      $prompt = t('Please type the i d of the phrase to be managed.');
      $phrase_id = vio_get_input($prompt, 15);
      if (($phrase_id === -1) || empty($phrase_id)) {
      	$msg = 'Error reading phrase id. Please contact your system administrator.';
      	eh_error($msg);
        vio_say(v($msg));
        return FALSE;
      }
eh_log("Current phrase id: $phrase_id");    
      $phrase_entry = vt_get_phrase($phrase_id);
eh_log("phrase_entry returned: " . print_r($phrase_entry, TRUE));
      if ($phrase_entry === -1) {
        $msg = "Processing error. Please contact your system administrator.";
        eh_error($msg);
        vio_say(v($msg));
        return FALSE;
      }
      else if (!($phrase_entry)) {
eh_log("Invalid phrase id: $phrase_id.");
        vio_say(v('There is no phrase associated with i d %phrase_id. Please try again.', array('%phrase_id' => $phrase_id)));
        continue;
      }
      else if ($phrase_entry['language'] != $voice_entry['language']) {
eh_log('Invalid language entry.');
        vio_say(v('Phrase %phrase_id is not in the appropriate language for the voice being recorded. Please chose a different one.', array('%phrase_id' => $phrase_id)));
      }
/**********      
      // similar to v(), split the phrase into an array of text chunks and arguments
      $speech_array = array();
      if (!$phrase_entry['placeholders']) {
        $speech_array[] = $phrase_entry['phrase'];
      }
      else {
  	    $token = '_wvm_' . mt_rand();
        foreach(array_keys($phrase_entry['placeholders']) as $key) {
          $tokenized[$key] = "$token$key$token";
        }
  	    $tokenized_string = strtr($phrase_entry['phrase'], $tokenized);
   	    $speech_array = explode($token, $tokenized_string);
   	    foreach ($speech_array as $index => $segment) {
   	      if (!$segment) {
   	        unset($speech_array[$index]);
   	      }
   	    }
      }
eh_log("speech_array: " . print_r($speech_array, TRUE));      
********/      
      $phrase_menu = TRUE;
      while ($phrase_menu) {
    
        $phrase_choices = array();
        $phrase_choices[1] = v('To hear the phrase in the current voice, press 1.');
        $phrase_choices[2] = v('To hear the computer-generated version of the phrase, press 2.');
        $phrase_choices[3] = v('To record the phrase with your voice, press 3.');
        $phrase_choices[4] = v('To delete the current recordings associated with this phrase, press 4.');
        $phrase_choices['*'] = v('To go to a different phrase, press the star key.');
        $phrase_choices['#'] = v('To go back to the main menu, press the pound key.');
        $choice = vio_menu($phrase_choices);
        switch ($choice) {
          default:
            eh_error("Error selecting phrase options (choice: $choice)");
            vio_say(v('Processing error. Please contact your system administrator.'));
            return FALSE;
        
          case 1: // hear current version of the phrase
            if ($voice_entry['language'] == 'en') {
              vio_beep();
              global $vt_voice_id;
              $vt_voice_id = $voice_entry['vid'];
              if ($phrase_entry['placeholders']) {
                vio_say(v($phrase_entry['phrase'], $phrase_entry['placeholders']));
              }
              else {
                $audio_phrase = vt_get_audio_phrase($phrase_entry['phrase'], $vt_voice_id);
eh_log("audio phrase: $audio_phrase");
                vio_say(vt_get_audio_phrase($audio_phrase));
              }
              vio_beep();
            }
            else {
vio_say('Function not implemented yet.');
// TODO: create a function that retrieves the original english phrase from the database; call vio_say(v()) for that phrase
            }
            break;
        
          case 2: // hear computer-generated version of the phrase
            if ($voice_entry['language'] == 'en') {
              vio_beep();
              vio_say($phrase_entry['phrase']); // function without the v()...
              vio_beep();
            }
            else {
vio_say('Function not implemented yet.');
// TODO: create a function to switch Festival's language/voice
            }
            break;
        
          case 3: // record the phrase
/*****
            $rc = _wvm_record_phrase($speech_array, $voice_entry);
            if (!$rc) {
              return FALSE;
            }
*****/
//----
  if (!$phrase_entry['placeholders']) {
    $prompt = t('Record the new phrase after the beep. When done, press the pound key.');
    $file_name = vio_record_audio($prompt);
    if ($file_name === -1) {
      eh_error('Error recording phrase.');
      vio_say("Processing error. Please contact your system administrator.");
      $rc = FALSE;
     }
     else {
       vt_put_audio_phrase($phrase_entry['phrase'], $voice_entry['vid'], $file_name);
       vio_say(v('The new phrase has been recorded.'));
       $rc = TRUE;
     }
  }
  else {
    // present the caller with the ids of all the phrases associated with the given sentence
    $phrase_entries = vt_get_sentence_phrases($phrase_entry['phrase'], $voice_entry['language']);
    if ($phrase_entries === -1) {
      eh_error("Processing error retrieving sentence phrases.");
      vio_say("Processing error. Please contact your system administrator.");
      $rc = FALSE;
    }
    else {
      $ids = '';
      for ($i = 0; $i < (count($phrase_entries) - 1); $i++) {
        $ids .= ', ' . $phrase_entries[$i]['pid'];
      }
      // if the sentence is made of multiple phrases, tell the user to record them individually
      if (count($phrase_entries) > 1) {
        $ids .= 'and ' . $phrase_entries[$i]['pid'];
        $prompt = v('The selected phrase is made of %count parts that will have to be individually recorded. Their phrase i dees are: %ids', array('%count' => count($phrase_entries), '%ids' => $ids));
        vio_say($prompt);
      }
      else {
//----
eh_log("phrase_entries: " . print_r($phrase_entries, TRUE));
            // if the sentence is made of a single phrase, record that phrase here
            $prompt = t('Record the new phrase after the beep. When done, press the pound key.');
            $file_name = vio_record_audio($prompt);
            if ($file_name === -1) {
              eh_error('Error recording phrase.');
              vio_say("Processing error. Please contact your system administrator.");
              $rc = FALSE;
            }
            else {
              vt_put_audio_phrase($phrase_entries[$i]['phrase'], $voice_entry['vid'], $file_name);
              vio_say(v('The new phrase has been recorded.'));
              $rc = TRUE;
            }
//----
         }
         $rc = TRUE;
    }
  }
  if (!$rc) {
    return FALSE;
  }
  break;

//----
        
        case 4: // delete phrase recordings
            vt_delete_audio_phrase($phrase_entry['phrase'], $voice_entry['vid'], $phrase_entry['placeholders']);
            vio_say(v('Phrase successfully deleted.'));
            break;
        
        case '*': // go to a different phrase
            continue 3;
        
        case '#': // go back 
            $phrase_menu = FALSE;
            $recording = FALSE;
            break;

        } // switch
      } // while phrase_menu
    } // while recording
  } // while main menu
    
  $prompt = v("Thank you for using the What's Up Voice Manager system. Goodbye.");
  vio_say($prompt);
   
  return $rc;
}

function _wvm_record_phrase($speech_array, $voice_entry) {
  if (count($speech_array) == 1) {
    $prompt = t('Record the new phrase after the beep. When done, press the pound key.');
    $file_name = vio_record_audio($prompt);
    if ($file_name === -1) {
      eh_error('Error recording new segment.');
      vio_say(v("Processing error. Please contact your system administrator"));
      $rc = FALSE;
     }
     vt_put_audio_phrase($speech_array[0], $voice_entry['vid'], $file_name);
     vio_say(v('The new phrase has been recorded.'));
     $rc = TRUE;
  }
  else {
  	// identify which segments need to be translated
  	$translate = array();
  	foreach ($speech_array as $segment) {
  	  if (($segment{0} != '%') && (!ctype_punct($segment)) && (!ctype_space($segment))) {
  	    $translate[] = $segment;
  	  }
  	}
eh_log("translate: " . print_r($translate, TRUE));
  	$count = count($translate);
  	if($count == 1) {
  	  vio_say(v('This phrase has one segment to be recorded.'));
      $prompt = t('Record the new segment after the beep. When done, press the pound key.');
      $file_name = vio_record_audio($prompt);
      if ($file_name === -1) {
        eh_error('Error recording new segment.');
        vio_say(v("Processing error. Please contact your system administrator"));
        $rc = FALSE;
      }
      vt_put_audio_phrase($translate[0], $voice_entry['vid'], $file_name);
      vio_say(v('The new segment has been recorded.'));
      $rc = TRUE;
  	}
  	else {
  	  vio_say(v('This phrase has %number segments to be recorded.', array('%number' => $count)));
  	  $index = 0;
  	  $recording = TRUE;
  	  while ($recording) {
  	    vio_say(v('Segment number %number.', array('%number' => ($index + 1))));
  	    $r = _wvm_record_segment($translate[$index], $voice_entry, TRUE);
  	    if ($r === -1) {
  	      $rc = FALSE;
  	      $recording = FALSE;
  	    }
  	    else if ($r === 0) {
  	      $rc = TRUE;
  	      $recording = FALSE;
  	    }
  	    else { // go to the next segment
  	      $index = ($index < ($count - 1))? ($index+1) : 0;
  	    }
  	  }
  	}
  }
  return $rc;
}

function _wvm_record_segment($segment, $voice_entry, $option_next = FALSE) {
eh_log("_wvm_record_segment($segment, $voice_entry, $option_next)");
  $menu = TRUE;
  while ($menu) {
    
    $choices = array();
    $choices[1] = v('To hear the segment in the current voice, press 1.');
    $choices[2] = v('To hear the computer-generated version of the segment, press 2.');
    $choices[3] = v('To record the segment with your voice, press 3.');
    $choices[4] = v('To delete the current recording associated with this segment, press 4.');
    if ($option_next) {
      $choices['*'] = v('To go to the next phrase segment, press the star key.');
    }
    $choices['#'] = v('To go back to the phrase selection menu, press the pound key.');
    $choice = vio_menu($choices);
    switch ($choice) {
      default:
        eh_error("Error selecting segment options (choice: $choice)");
        vio_say(v('Processing error. Please contact your system administrator.'));
        return -1;
        
      case 1: // hear current version of the segment
        if ($voice_entry['language'] == 'en') {
          vio_beep();
          global $vt_voice_id;
          $vt_voice_id = $voice_entry['vid'];
          vio_say(v($segment));
          vio_beep();
        }
        else {
vio_say('Function not implemented yet.');
// TODO: create a function that retrieves the original english phrase from the database; call vio_say(v()) for that phrase
        }
        break;
        
      case 2: // hear computer-generated version of the segment
        if ($voice_entry['language'] == 'en') {
          vio_beep();
          vio_say($segment); // function without the v()...
          vio_beep();
        }
        else {
vio_say('Function not implemented yet.');
// TODO: create a function to switch Festival's language/voice
        }
        break;
        
      case 3: // record the segment
        $prompt = t('Record the new segment after the beep. When done, press the pound key.');
        $file_name = vio_record_audio($prompt);
        if ($file_name === -1) {
          eh_error('Error recording new segment.');
          vio_say(v("Processing error. Please contact your system administrator"));
          return -1;
        }
        vt_put_audio_phrase($segment, $voice_entry['vid'], $file_name);
        vio_say(v('The new segment has been recorded.'));
        break;
       
      case 4: // delete segment recordings
        vt_delete_audio_phrase($segment, $voice_entry['vid']);
        vio_say(v('Segment recording successfully deleted.'));
        break;
       
      case '*': // go to next segment 
        $menu = FALSE;
        $rc = 1;
        break;

      case '#': // go back to phrase menu
        $menu = FALSE;
        $rc = 0;
        break;

    } // switch
  } // while menu

  return $rc; 
}

function wvm_select_voice($allow_create = FALSE) {
  $menu = TRUE;
  while ($menu) {

    // select available voices from the database
    $voices = vt_get_voices();
    if ($voices === FALSE) {
      return -1;
    }
    
    // create a voice-selection menu
    $voice_choices = array();
    foreach ($voices as $index => $voice_entry) {
      $voice_choices[$index + 1] = v('For %voice_name, type %voice_id.', array('%voice_name' => ('*' . $voice_entry['file_basename']), '%voice_id' => ($index+1)));
    }
    if ($allow_create) {
      $voice_choices['*'] = v('To create a new voice, press the star key.');
      if ($voices) {
        $voice_choices['#'] = v('To change the name of an existing voice, press the pound key.');
      }
    }
    // play voice options to caller
    vio_beep();
    vio_say(t('Voice selection menu.'));
    if ($voices) {
      vio_say(v("Please select the voice you would like to manage."));
    }
    $choice = vio_menu($voice_choices);
    switch ($choice) {
      case -1:
        eh_error("Error selecting voice options (choice: $choice)");
        vio_say(v('Processing error. Please contact your system administrator.'));
        $rc = -1;
        $menu = FALSE;
        break;

      case '*': // create new voice
        $prompt = t("Please record the name of the new voice after the beep.  When done, press the pound key.");
        $file_name = vio_record_audio($prompt);
        if ($file_name == -1) {
          $rc = -1;
          $menu = FALSE;
        }
        else {
          $language_choices = array();
          $language_choices[1] = v('Please select the base language of the new voice. For English, press 1');
          $language_choices[2] = v('For Spanish, press 2.');
          $choice = vio_menu($language_choices);
          switch($choice) {
            default: // invalid option
            case -1: // processing error
              eh_error("Error selecting language options (choice: $choice)");
              $rc = -1;
              $menu = FALSE;
              $prompt = v('Processing error. Please contact your system administrator.');
              vio_say($prompt);
              continue 3;
    
           case 1: // English
             $language = 'en';
             break;

           case '2': // Spanish
             //$language = 'es';
             //break;
             vio_say('I am sorry, but this feature is not currently available');
             continue 3;
    
          }
eh_log("About to call: vt_add_voice($language, $file_name)");
          $r = vt_add_voice($language, $file_name);
          if (!$r) {
          	$rc = -1;
          	$menu = FALSE;
          }
          else {
            vio_say(v('New voice successfully created.'));
          }
        }
        break;
        
      case '#': // change the name of an existing voice
        $voice_menu = array();
        foreach ($voices as $index => $voice_entry) {
          $change_name_menu[$index + 1] = v('For %voice_name, type %voice_id.', array('%voice_name' => ('*' . $voice_entry['file_basename']), '%voice_id' => ($index+1)));
        }
        $change_name_menu['#'] = v('To go back to the main menu, press the pound key.');
        $change_name = TRUE;
        while ($change_name) {
          vio_say(v("Please select the voice whose name you would like to change."));
          $change_name_choice = vio_menu($change_name_menu);
eh_log("change_name_choice: $change_name_choice");          
          switch ($change_name_choice) {
            case -1:
              eh_error("Error selecting voice name options (choice: $change_name_choice)");
              vio_say(v('Processing error. Please contact your system administrator.'));
              $rc = -1;
              $menu = FALSE;
              $change_name = FALSE;
              break;

            case '#': // go back to main menu
              $change_name = FALSE;
              break;
              
            default: // record the new name
              $prompt = t("Please record the new name of the voice after the beep.  When done, press the pound key.");
              $file_name = vio_record_audio($prompt);
              if ($file_name == -1) {
                $rc = -1;
                $menu = FALSE;
                $change_name = FALSE;
              }
              else {
              	$voice_id = $voices[$change_name_choice - 1]['vid'];
                $r = vt_change_voice_name($voice_id, $file_name);
                if (!$r) {
                  $rc = -1;
                  $menu = FALSE;
                  $change_name = FALSE;
                }
                else {
                  vio_say(t("The voice name has been changed."));
                  $change_name = FALSE;
                }
              }
              break;
          }
        }
        break;
    
      default: // voice selected
        $rc = $voices[$choice - 1];
        $menu = FALSE;
        break;
    }
  } // while menu
eh_log("rc: $rc");  
  return $rc;
}


?>
