<?php
/* $Id$

/**
 * @file
 * Requests and executes Voip Drupal scripts
 *
 */


// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

global $configuration_file;
if (!isset($configuration_file)) {
    $configuration_file = "whatsup.ini";
}
$whatsup_config = parse_ini_file($configuration_file, TRUE);
$whatsup_config = $whatsup_config['whatsup'];


//TODO: remove explicit dependencies to agi...
global  $agi;
if( !isset( $agi)) {
  global $configuration_file;
  if (!isset($configuration_file)) {
    $configuration_file = "phpagi.ini";
  }
  $phpagi_config = parse_ini_file($configuration_file, TRUE);
  $phpagi_config = $phpagi_config['phpagi'];
  $phpagi_config_dir = isset($phpagi_config['phpagi_config_dir'])? $phpagi_config['phpagi_config_dir'] : "/etc/asterisk/phpagi.conf";
	
   require_once('phpagi.php');
   $agi = new AGI($phpagi_config_dir);
}

/*
 * Stores the URL of the xmlrpc.php file associated with server from which 
 *   information about specific voip extensions is to be retrieved
 * 
 * @var array
 * @access private
 */
global $wu_voip_server;
 
/*
 * Stores Drupal name, password and caller id of current user.
 * 
 * @var array
 * @access private
 * 
 * Holds caller id, name and password of current user
 */
global $wu_user_info;




// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'voip_api.inc',
                'voip_client.inc', 'voip_event_client.inc', 'voip_group_client.inc',
                'voip_io.inc', 'voip_voicemail_client.inc', 'audio_server_api.inc',
                'error_handler.inc', 'whatsup_voice_manager.inc', 'xmlrpc.inc') as $file) {
    require_once($file);
}


// -----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------

define('WU_DEFAULT_VOICE_ID', $whatsup_config['wu_default_voice_id']);
define('WU_BEEP_FILE', isset($whatsup_config['wu_beep_file'])?$whatsup_config['wu_beep_file']:'beep');


define('WU_SUCCESS', 1);
define('WU_GO_TO_MAIN_MENU', 2);
define('WU_GO_TO_EXTENSION', 3);
define('WU_GO_BACK', 4);
define('WU_PROCESSING_FAILURE', -1);


// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

/*
 * Asterisk script handler
 *
 * @param $server
 *   URL of the xmlrpc.php file associated with the Voip Drupal server
 *   from which to retrieve the scripts
 *
 * @param $script_name
 *   name of the script to be processed
 *
 * @param $options
 *   array with script options 
 *
 * @return
 *   boolean TRUE indicates success.  If FALSE, check $eh_error_msg() for explanation.
 */
function asterisk_script_handler($server, $input_script_name, $input_options=array()) {
eh_log("entering asterisk_script_handler($server, $input_script_name, $input_options)");

  // set the current voice of the system
  vt_set_voice(WU_DEFAULT_VOICE_ID);
  

  $processing = TRUE;
  $rc = TRUE;
  $script_name = $input_script_name;
  $options = $input_options;

  while($processing){

    // retrieve script
    $request_id = 'voip_get_script';
    $options['script_name'] = $script_name;
eh_log("about to call voip_process_request($server, $request_id, " . print_r($options,TRUE) . ")");
    $result = voip_process_request($server, $request_id, $options);
eh_log('so far, so good');
eh_log('result: ' . print_r($result, TRUE));

    // check for processing errors
    if(voip_api_error()){
      $processing = FALSE;
      $rc['status'] = FALSE;
      $error_msg = "(asterisk script handler): Error retrieving $script_name from $server: "
        . voip_error_message();
eh_log($error_msg);
      $rc['error_msg'] = $error_msg;
      continue;
    }


    // parse script
    $script = $result['script_contents'];
    $parser = xml_parser_create();
    xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 1);
    xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
    xml_parse_into_struct($parser, $script, $values, $tags);
    xml_parser_free($parser);

    // loop through the script elements
    for ($i=0, $stop = FALSE; $i<count($values) && !$stop; $i++) {
      $element = $values[$i];
      $type = $element['type'];
      if(($type == 'open') || ($type == 'complete')) {
eh_log("processing tag: " . $element['tag'] . "\n");
        switch($element['tag']) {
          default:
// vio_say($element['tag']);
            break;

          case "GOTO":
            $script_name = $element['value'];
            $stop = TRUE;
            break;

          case "HANGUP":
            // hangup command
eh_log("about to hangup");
            $processing = FALSE;
            $stop = TRUE;
            break;

          case "SAY":
            $i++;
            $element = $values[$i];
            if($element['tag'] == 'TEXT') {
              vio_say($element['value']);
eh_log("say text: " . $element['value']);
            } else {
              vio_say('*' . $element['value']);
eh_log("play file: " . $element['value']);
            }
            break;
        }
      }
    }

    // reached the end of the script
    if($i == count($values)){
eh_log("end of the script");
      $processing = FALSE;
    }
  }

  // return
  return $rc;
}




// =================================
function old_asterisk_script_handler($server, $script_name, $options) {
eh_log("asterisk_script_handler($server, $script_name, $options)");

  global $agi;
  
  global $wu_user_info;
  global $wu_voip_server;

  // Initialize global variables;
  $wu_user_info = array();
  $wu_user_info['user_name'] = NULL;
  $wu_user_info['password'] = NULL;
// TODO: replace the following line by wu_user_info_from_caller_id(), which would handle autologin
  $wu_user_info['caller_id'] = array('name' => $agi->request['agi_calleridname'], 
                                     'number' => $agi->request['agi_callerid']);

  $wu_voip_server = $voip_server;

  // set the current voice of the system
  vt_set_voice(WU_DEFAULT_VOICE_ID);
  
  // set the current beep sound of the system
  global $vio_beep_file;
  $vio_beep_file = WU_BEEP_FILE;

  // record statistics
  // TODO: create a log function at the server
//  voip_log($wu_voip_server, $wu_user_info, 'user', 'dialed in');
  
vio_say('hi. this is a test');

  // start interacting with the caller
  vio_beep();

  vio_say(v('Welcome to the Voip Drupal system.'));

  // start music on hold
  vio_set_music_on_hold_class('whatsup');
  vio_start_music_on_hold();

  $choices = array();
  $choices[1] = v('Main menu. To go to a specific extension, press 1');// 'To go to a specific extension, press 1.');
  $choices[2] = v('For community audioblogs, press 2.'); // v('For community announcements, press 2.');
  $choices[3] = v('For the calendar of community events, press 3.');
  $choices[4] = v('For information about the Whats Up system, press 4.');
  $choices['*'] = v('To go to your personal area, press the star key.');
  
  $rc = TRUE;
  $main_menu = TRUE;

  while ($main_menu) {
    // initiatilize dynamic items of the main menu
    if (isset($wu_user_info['user_name'])) {
      $choices['#'] = v('To logout, press the pound key.');
    } 
    else { 
      $choices['#'] = v('To log in, press the pound key.');
    }

    // main menu
    vio_beep();
    $choice_processing = TRUE;
    $destin_extension = NULL;
    $choice = vio_menu($choices, 5);
    if (is_null($choice)) {
      $choice_processing = FALSE;
      $main_menu = FALSE;
    }
    while ($choice_processing) {
      switch($choice) {
        default: // invalid option
        case -1: // processing error
          eh_error("Error selecting main menu options (choice: $choice)");
          $rc = FALSE;
          $choice_processing = FALSE;
          $main_menu = FALSE;
          $prompt = v('Processing error. Please contact your system administrator.');
          vio_say($prompt);
          break;
    
        case 1: // go to a specific individual or group extension
          $rc = _whatsup_extension($destin_extension);
          $destin_extension = NULL;
          if ($rc == -1) {
            $rc = FALSE;
            $main_menu = FALSE;
            $prompt = v("Processing error. Please contact your system administrator.");
            vio_say($prompt);
          }
          $choice_processing = FALSE;          
          break;

        case '2': // go the community audioblog area
          $r = _whatsup_community_audioblog_area();
          if ($r === -1) {
            $rc = FALSE;
            $main_menu = FALSE;
          }
          $choice_processing = FALSE;
          break;
        
        case '3': // community events calendar
          $r = _whatsup_event_area();
          if ($r == WU_PROCESSING_FAILURE) {
            $rc = FALSE;
            $main_menu = FALSE;
          }
          $choice_processing = FALSE;
          break;  
    
        case '4': // provide information about the What's Up initiative
          $r = _whatsup_about_the_initiative();
          $choice_processing = FALSE;
          break;
	
        case '*': // go to the caller's personal extension
          $rc = _whatsup_personal_extension();
          $choice_processing = FALSE;
          if ($rc['error_code'] == WU_PROCESSING_FAILURE) {
            $rc = FALSE;
            $main_menu = FALSE;
          }
          else if ($rc['error_code'] == WU_GO_TO_EXTENSION) {
            $choice_processing = TRUE;
            $choice = 1; // go to specific extension
            $destin_extension = $rc['extension_number'];
          }
          break;

        case '#':
          if (!empty($wu_user_info['user_name'])) {
            $prompt = v("Logging out.");
            vio_say($prompt);
            voip_log($wu_voip_server, $wu_user_info, 'user', 'logout');
            $wu_user_info['user_name'] = NULL;
          } 
          else { 
            $r = _whatsup_login();
            if ($r != 1) {
              $rc = FALSE;
              $main_menu = FALSE;
              $prompt = v('Processing error trying to log user in.');
              vio_say($prompt);
            }
          }
          $choice_processing = FALSE;          
          break;
       } // switch
    } // while $choice_processing
  } // while main_menu
  
  $prompt = v('Thank you for using the Voip Drupal system. Goodbye.');
  vio_say($prompt);
   
  return $rc;
}

/*
 * What's Up community audioblog area.
 *
 * @return
 *   integer -1 indicates processing error, 0 indicates that the user cancelled the operation, 1 indicates success.  In case of error, check $eh_error_msg() for explanation.
 */
function _whatsup_community_audioblog_area() {
  global $wu_voip_server;
  global $wu_user_info;
  
  $rc = 1;

  $choices = array();
  $choices[1] = v('For audioblog entries published on the front page of the website, press 1.');
  $choices[2] = v('For public audioblog entries published by any user of the system, press 2.');
  $choices[3] = v('For audioblog entries from specific categories, press 3.');
//  $choices[4] = v('For audioblog entries posted on a specific time period, press 4.');
  $choices['#'] = v('To go back to the main menu, press the pound key.');

  $audioblog_menu = TRUE;
  while ($audioblog_menu) {

      vio_beep();
      $choice = vio_menu($choices);
      switch($choice) {
        case -1:
          eh_error("Error selecting community audioblog menu options");
          $rc = -1;
          $audioblog_menu = FALSE;
          break;
  
        case '#':
          $rc = 0;
          $audioblog_menu = FALSE;
          break;

        case 1: // listen to front page audioblog entries
          $query_info = array();
          $query_info['status'] = 1; // retrieve only published audioblog entries
          $query_info['promote'] = 1; // promoted to the front page
          $query_info['name'] = NULL;
          $query_info['categories_or'] = NULL;
          $query_info['categories_and'] = NULL;
          $query_info['unread_entries_only'] = FALSE;
          $prompt = v("Retrieving audioblog entries from the server.");
          vio_say($prompt);
          $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries available at this time. Please try again later.");
          $rc = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
          switch ($rc) {
          	default:
          	case -1: // processing error
              $prompt = v("Error retrieving audioblog entries. Please contact your system administrator.");
              vio_say($prompt);
              $rc = -1;
              $audioblog_menu = FALSE;
              break;
              
            case 0: // go back to main menu
              $rc = 0;
              $audioblog_menu = FALSE;
              break;
              
            case 1: // keep going
              break;
          }
          break;

        case 2: // listen to public audioblog entries from anyone
          $query_info = array();
          $query_info['status'] = 1; // retrieve only published audioblog entries
          $query_info['promote'] = NULL; // promoted or not to the front page
          $query_info['name'] = NULL;
          $query_info['categories_or'] = NULL;
          $query_info['categories_and'] = NULL;
          $query_info['unread_entries_only'] = FALSE;
          $prompt = v("Retrieving audioblog entries from the server.");
          vio_say($prompt);
          $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries available at this time. Please try again later.");
          $rc = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
          switch ($rc) {
          	default:
          	case -1: // processing error
              $prompt = v("Error retrieving audioblog entries. Please contact your system administrator.");
              vio_say($prompt);
              $rc = -1;
              $audioblog_menu = FALSE;
              break;
              
            case 0: // go back to main menu
              $rc = 0;
              $audioblog_menu = FALSE;
              break;
              
            case 1: // keep going
              break;
          }
          break;

        case 3: // listen to audioblog entries from a specific category
          $query_info = array();
          $query_info['status'] = 1;
          $query_info['uid'] = NULL;
          $category_list = asa_get_audio_category_list($wu_voip_server);
          $accept_none = FALSE;
          $accept_multiple = TRUE;
          $accept_any = TRUE;
          $accept_any_txt = "To select entries from any of the existing categories, press %key.";
          $r = _whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $accept_any_txt);
          if (!$r->success) {
            $prompt = v("Error selecting categories from the server. Please contact your system administrator.");
            vio_say($prompt);
            $rc = -1;
            $audioblog_menu = FALSE;
            break;
          }
          $query_info['categories_or'] = $r->selected_ids;
eh_log("categories_or: " . serialize($query_info['categories_or']));
          $query_info['categories_and'] = NULL;
          $query_info['unread_entries_only'] = FALSE;
          $prompt = v("Retrieving audioblog entries from the server.");
          vio_say($prompt);
          $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries currently available for the specified categories. Please try again later.");
          $rc = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
           switch ($rc) {
          	default:
          	case -1: // processing error
              $prompt = v("Error retrieving audioblog entries. Please contact your system administrator.");
              vio_say($prompt);
              $rc = -1;
              $audioblog_menu = FALSE;
              break;
              
            case 0: // go back to main menu
              $rc = 0;
              $audioblog_menu = FALSE;
              break;
              
            case 1: // keep going
              break;
          }
          break;

        default:
          $prompt = v("Function not implemented, yet");
          vio_say($prompt);
          break;
      }
    }

  return $rc;
}

/*
 * Provide information about the What's Up initiative'.
 *
 * @return
 *   integer 1 indicates success.
 */
function _whatsup_about_the_initiative() {
  
eh_log("whatsup_about_the_initiative()");

  vio_say(v("Please record a brief description about the What's Up system here'."));
  return 1;
}

/*
 * Allows users to access and configure their personal space within the system.
 *
 * @return
 * 
define('WU_SUCCESS', 1);
define('WU_GO_TO_MAIN_MENU', 2);
define('WU_GO_TO_EXTENSION', 3);
define('WU_PROCESSING_FAILURE', -1);

 *   array with 'error_code' set to:
 *     WU_SUCCESS in case of success
 *     WU_PROCESSING_FAILURE in case of processing failure
 *     WU_GO_TO_MAIN_MENU to go back to the main menu
 *     WU_GO_TO_EXTENSION to go to the extension number stored in 'extension_number'
 * 
 *   In case of WU_PROCESSING_FAILURE, check $eh_error_msg() for explanation.
 */
function _whatsup_personal_extension() {
eh_log("whatsup_personal_extension()");

  global $wu_voip_server;
  global $wu_user_info;
  
  $r = _whatsup_login();
  if ($r != 1) {
  	$rc = array();
  	$rc['error_code'] = ($r == 0)?WU_SUCCESS:WU_PROCESSING_FAILURE;
    return $rc;
  }

  // retrieve user extension
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $wu_user_info['extension_number']);
  if ($result['error_code'] == VOIP_SUCCESS) {
    $extension_info = $result['extension_info']; 
  }
  
eh_log("extension_info: " . print_r($extension_info, TRUE));

  $has_active_mailbox = FALSE;
    
  $stop = FALSE;
  while (!$stop) {
    // determine the user name
    if (isset($extension_info['file_audio_name'])) {
      $user_name = '*' . $extension_info['file_audio_name'];
    }
    else {
      $user_name = t('user extension %x', array('%x' => $extension_info['number']));
    }
    
eh_log("user_name: " . print_r($user_name, TRUE));

    // welcome caller
    vio_beep();
    $prompt = v("Personal area for %user_name", array('%user_name' => $user_name));
    vio_say($prompt);
    
    
    // Make sure user has a mailbox and, if so, present the number of new messages in the user mailbox
    
    $rc = voip_voicemail_mailbox_get_info($wu_voip_server, $wu_user_info, $extension_info['number']);
    if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST)) {
      eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
      return array('error_code' => WU_PROCESSING_FAILURE);
    }
    
    if ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST) {

      $mailbox_info = $rc['mailbox_info'];
    
      if ($mailbox_info['is_active']) {

        $has_active_mailbox = TRUE;
        $rc = voip_voicemail_mailbox_get_number_of_messages($wu_voip_server, $wu_user_info, $extension_info['number']);
        if ($rc['error_code'] != VOIP_SUCCESS) {
          eh_error('Error retrieving number of messages from mailbox: ' . $rc['error_msg']);
          return array('error_code' => WU_PROCESSING_FAILURE);
        }  
    
        $new_msg_count = $rc['count_new_messages'];
        $new_msg_text = ($new_msg_count <= 0)?'':(($new_msg_count == 1)?t('1 new message'):t('%count new messages  ', array('%count' => $new_msg_count)));
        if (!empty($new_msg_text)){
          $prompt = v('You have %new_text in your voicemail box.', array('%new_text' => $new_msg_text));
          vio_say($prompt);
        }
      }
    }
      
    // now present the personal area menu
    
    $choices = array();
    if ($has_active_mailbox) {
      $choices[1] = v('To check your voicemail messages, press 1.');
    }
    $choices[2] = v('To record a new audioblog entry, press 2.');
    $choices[3] = v('To listen to your existing audioblog entries, press 3.');
    // $choices[4] = v('To create and manage community events, press 4.');
    $choices[5] = v('To create and manage groups, press 5.');
    $choices[6] = v('To change your personal settings, press 6.');
    $choices['#'] = v('To go back to the main menu, press the pound key.');
      	
    $choice = vio_menu($choices);
    switch($choice) {

      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting personal menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = array('error_code' => WU_PROCESSING_FAILURE);
        $stop = TRUE;
        break;

      case 1:
        $rc2 = _whatsup_voicemail_check_messages($wu_voip_server, $extension_info['number']);
        switch ($rc2) {
          case -1:
          default:
            $rc = array('error_code' => WU_PROCESSING_FAILURE);
            $stop = TRUE;
            break;
          case 2: // go back to main menu
            $rc = array('error_code' => WU_GO_TO_MAIN_MENU);
            $stop = TRUE;
            break;
          case 1: // success: keep going
            break;
        } 
        break;

      case 2: // record a new audioblog entry
        $rc = _whatsup_audioblog_create($extension_info);
        if ($rc == -1) {
          $prompt = v("Processing error recording audioblog. Please contact your system administrator.");
           vio_say($prompt);
           $rc = array('error_code' => WU_PROCESSING_FAILURE);
           $stop = TRUE;
        }
        break;

       case 3:
 // TODO: is there any way to make a personal audio entry private to the user?
         $query_info = array();
	     $query_info['status'] = 1;
	     $query_info['uid'] = $extension_info['uid'];
	     $query_info['categories_or'] = NULL;
	     $query_info['categories_and'] = NULL;
	     $query_info['unread_entries_only'] = FALSE;
         $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries recorded by you. Please try again later.");
         $r = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
         switch ($r) {
	       default:
	       case -1: // processing error
	         $prompt = v("Error retrieving audioblog entries. Please contact your system administrator.");
	         vio_say($prompt);
             $rc = array('error_code' => WU_PROCESSING_FAILURE);
	         $stop = TRUE;
	         break;
	              
	       case 0: // cancel. go back to the main menu
             $rc = array('error_code' => WU_GO_TO_MAIN_MENU);
	         $stop = TRUE;
	         break;
	              
	       case 1: // keep going
	         break;
	    }
        break;

      case 4: // manage community events
        $is_organizer = TRUE;
        $calling_extension_name = t("your personal area");
        $rc = _whatsup_event_management_area($extension_info, $is_organizer, $calling_extension_name);
        if ($rc == -1) {
          $rc = array('error_code' => WU_PROCESSING_FAILURE);
          $stop = TRUE;
        }
        break;
 
      case 5:
        $r = _whatsup_personal_groups_management_area($extension_info); 
        if ($r['error_code'] == WU_PROCESSING_FAILURE) {
          $rc = array('error_code' => WU_PROCESSING_FAILURE);
          $stop = TRUE;
        }
        else if ($r['error_code'] == WU_GO_TO_EXTENSION) {
          $rc = $r;
          $stop = TRUE;
        }
        break;

      case 6: 
        vio_say('Personal settings menu');
        $choices2 = array();
        $choices2[1] = v('To change your phone password, press 1');
        $choices2[2] = v('To record your name, press 2');
        $choices2[3] = v('To record a welcome message for your extension, press 3');
        $choices2[4] = v('To record an audio description about yourself, press 4');
        $choices2[5] = v('To change the categories or areas of interest associated with your extension, press 5.');
        $choices2[6] = v('To change the voice of the system, press 6.');
        $choices2['#'] = v('To go back to your personal area, press the pound key');
        $choice2 = vio_menu($choices2);
        switch($choice2) {
          case 1:
            $r = _whatsup_change_phone_pin();
            if ($r == -1) {
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            break;

          case 2;
            $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_name');
            if ($r == -1) {
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            break;
            
          case 3:
            $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_greetings');
            if ($r == -1) {
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            break;
            
          case 4:
            $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_description');
            if ($r == -1) {
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            break;
            
          case 5: // change extension categories
            $category_list = voip_get_category_list($wu_voip_server);
            $accept_none = TRUE;
            $accept_multiple = TRUE;
            $accept_any = FALSE;
            $any_categories_txt = NULL;
            $r = _whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $any_categories_txt);
            if (!$r->success) {
              $prompt = v("Error selecting categories from the server. Please contact your system administrator.");
              vio_say($prompt);
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            $category_ids =& $r->selected_ids;
            $r = voip_extension_set_categories($wu_voip_server, $wu_user_info, $extension_info['number'], $category_ids);
            if ($r['error_code'] != VOIP_SUCCESS) {
              vio_say(v('Processing error. Please contact your system administrator.'));
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              $stop = TRUE;
            }
            vio_say(v('The categories associated with your extension have been succesfully updated.'));
            break;
            
          case 6: // change the voice of the system
            _whatsup_select_voice();
            break;
            
          case '#':
            break;
            
          default:
            eh_error("Error selecting personal settings menu options (choice: $choice)");
            $prompt = v('Processing error. Please contact your system administrator.');
            vio_say($prompt);
            $rc = array('error_code' => WU_PROCESSING_FAILURE);
            $stop = TRUE;
            break;
          
        }
        break;

      case '#':
        $rc = array('error_code' => WU_GO_TO_MAIN_MENU);
        $stop = TRUE;
        break;
    } // switch  
  } // while
  
  return $rc;
}

function _whatsup_personal_groups_management_area($extension_info) {
  global $wu_user_info;
  global $wu_voip_server;
  
  $download_user_groups = TRUE;
  $stop = FALSE;
  while (!$stop) {
    vio_beep();
    $prompt = v('Group management menu.');
    vio_say($prompt);

    // retrieve list of groups the user is involved with
    if ($download_user_groups) {
      vio_say(v('Retrieving group membership information from the server.'));
      $rc = voip_group_get_user_groups($wu_voip_server, $wu_user_info, $extension_info['uid']);
      if ($rc['error_code'] !== OG_XMLRPC_SUCCESS) {
        eh_error("Error retrieving user groups for user " . $extension_info['uid']);
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        return array('error_code' => WU_PROCESSING_FAILURE);
      }
eh_log('user groups: ' . print_r($rc, TRUE));
      $subscriptions = $rc['subscriptions'];

      $managed_groups = array();
      foreach ($subscriptions as $subscription) {
        if ($subscription['is_admin']) {
          $managed_groups[] = $subscription;
        }
      }
      
      $download_user_groups = FALSE;
    }

    // present the personal group management menu
    $choices = array();
    $choices[1] = v('To list the groups you are a member of, press 1');
    $choices[2] = v('To list only the groups you manage, press 2');
    $choices[3] = v('To create a new group, press 3');
//    $choices[4] = v('To delete any of the groups you manage, press 4');
    $choices['#'] = v('To go back to your personal area, press the pound key');
    $choice = vio_menu($choices);
    switch($choice) {
      case 1:
      case 2:
        if ($choice == 1) {
          if (empty($subscriptions)) {
            vio_say('I am sorry, but you are not currently enrolled in any group.');
            break;
          }
          $count = count($subscriptions);
          $prompt = ($count == 1)?v('You are currently only enrolled in ')
                                 :v('You are currently enrolled in the following %c groups', array('%c' => $count));
          $group_list =& $subscriptions;
        }
        else {
          if (empty($managed_groups)) {
            vio_say(v('I am sorry, but you are not currently managing any group.'));
            break;
          }
          $count = count($managed_groups);
          $prompt = ($count == 1)?v('You are currently only managing ')
                                 :v('You are currently managing the following %c groups', array('%c' => $count));
          $group_list =& $managed_groups;
        }
        vio_say($prompt);
        foreach ($group_list as $index => $subscription) {
          $group_name = '';
          $group_extension = $subscription['extension_info'];
          if (isset($group_extension['file_audio_name'])) {
            $group_name = '*' . $group_extension['file_audio_name'];
          }
          else if (isset($group_extension['name'])) {
            $group_name = $group_extension['name'];
          }
          $group_choices_menu = array();
          $group_choices_menu[1] = v('%name at extension %x. To go to this extension, press 1.', array('%name' => $group_name, '%x' => $group_extension['number']));
          if (($count > 1) && ($index < ($count - 1))){
            $group_choices_menu[9] = v('To go to the next group of the list, press 9');
          }
          $group_choices_menu['#'] = v('To go back to you personal area, press the pound key');
          $group_choice = vio_menu($group_choices_menu);
          switch ($group_choice) {
            default:
              eh_error("Error selecting personal group management menu options (choice: $choice)");
              vio_say(v('Processing error. Please contact your system administrator.'));
              $rc = array('error_code' => WU_PROCESSING_FAILURE);
              return $rc;
            case 1: // go to the group's extension
              $rc = array('error_code' => WU_GO_TO_EXTENSION, 'extension_number' => $group_extension['number']);
              return $rc;
            case 9: // go to next group of the list
              break;
            case '#': // go back to personal area
              $rc = array('error_code' => WU_SUCCESS);
              return $rc;
          }         
        }
        break;
            
      case 3: // create the new group
        $r = _whatsup_group_create_group();
        if ($r == -1) {
          $rc = array('error_code' => WU_PROCESSING_FAILURE);
          $stop = TRUE;
        }
        $download_user_groups = TRUE;
        break;

      case '#':
        $stop = TRUE;
        $rc = array('error_code' => WU_SUCCESS);
        break;
            
      default:
        eh_error("Error selecting personal group management menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = array('error_code' => WU_PROCESSING_FAILURE);
        $stop = TRUE;
        break;
          
    }        	
  }
  return $rc;
}

/*
 * Check voicemail messages for the specified mailbox.
 *
 * @param $context
 *   context of the extension.
 *
 * @param $extension
 *   context of the extension.
 *
 * @return
 *   integer. -1 in case of processing failure, 1 in case of success, 2 to go back to the main menu.
 */
function _whatsup_voicemail_check_messages($voicemail_server, $mailbox_number) {
eh_log("Entering _whatsup_voicemail_check_messages($voicemail_server, $mailbox_number)");

  global $wu_user_info;

  // Make sure the mailbox exists and is active
  $rc = voip_voicemail_mailbox_get_messages($voicemail_server, $wu_user_info, $mailbox_number);
eh_log('mailbox info: ' . serialize($rc));

  if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_NOT_ACTIVE)) {
    eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
    $prompt = v("Processing error reading mailbox %mailbox_number.", array('%mailbox_number' => $mailbox_number));
    vio_say($prompt);
    return -1;
  }
    
  if ($rc['error_code'] == VOIP_VOICEMAIL_MAILBOX_NOT_ACTIVE) {
    $prompt = v('I am sorry, but the specified mailbox is not currently active.');
    vio_say($prompt);
    return 1;
  }

  // separate new messages from saved messages
  $new_msgs = array();
  foreach($rc['meta_messages'] as $key => $msg_info) {
    if ($msg_info['is_new']) {
      $new_msgs[] = $msg_info;
      unset($rc['meta_messages'][$key]);
    }
  }
  $saved_msgs = $rc['meta_messages'];
  
  // say number of messages
  $new_msg_count = count($new_msgs);
  $saved_msg_count = count($saved_msgs);
  $new_msg_text = ($new_msg_count <= 0)?'':(($new_msg_count == 1)?t('1 new message'):t('%count new messages  ', array('%count' => $new_msg_count)));
  $saved_msg_text = ($saved_msg_count <= 0)?'':(($saved_msg_count == 1)?t('1 saved message'):t('%count saved messages  ', array('%count' => $saved_msg_count)));
  $prompt = '';
  if (!empty($new_msg_text) || !empty($saved_msg_text)){
    $and_text = ((!empty($new_msg_text)) && (!empty($saved_msg_text)))?t('and'):'';
    $prompt = v('You have %new_text %and_text %saved_text in your voicemail box.', array('%new_text' => $new_msg_text, '%and_text' => $and_text, '%saved_text' => $saved_msg_text));
  }
      
  if (empty($prompt)) {
    $prompt = v('You have no messages in your mailbox.');
    vio_say($prompt);
    return 1;
  }
  else {
    vio_say($prompt);
  }
  
  
  // now play the new messages
 
  $index = 1;
  foreach ($new_msgs as $msg_info) {
    $prompt = v("New message number %i", array('%i' => $index++));
    vio_say($prompt);
    $rc = _whatsup_voicemail_message_play($voicemail_server, $msg_info); 
eh_log("value returned by msg play: $rc");
    switch ($rc) {
      case -1:
        return -1;
      case 4: // skip to next message
      case 1: // success, move to next message
        continue;
      case 2: // return to personal area
        return 1;
      case 3: // go back to main menu
        return 2;
      default:
        eh_error("Processing error reading result of voicemail message play: $rc.");
        $prompt = v("Processing error. Please contact your system administrator.");
    	vio_say($prompt);
    	return -1;
    }
  }
  if (!empty($new_msgs)) {
    $prompt = v("End of new messages.");
    vio_say($prompt);
  }

  // now play the saved messages
  $index = 1;
  foreach ($saved_msgs as $msg_info) {
    $prompt = v("Saved message number %i", array('%i' => $index++));
    vio_say($prompt);
    $rc = _whatsup_voicemail_message_play($voicemail_server, $msg_info); 
eh_log("value returned by msg play: $rc");
    switch ($rc) {
      case -1:
        return -1;
      case 4: // skip to next message
      case 1: // success, move to next message
        continue;
      case 2: // return to personal area
        return 1;
      case 3: // go back to main menu
        return 2;
      default:
        eh_error("Processing error reading result of voicemail message play: $rc.");
        $prompt = v("Processing error. Please contact your system administrator.");
    	vio_say($prompt);
    	return -1;
    }
  }
  if (!empty($saved_msgs)) {
    $prompt = v("End of messages.");
    vio_say($prompt);
  }

  return 1;
  
}

/*
 * Convert date to a friendly format
 */
function _whatsup_friendly_date_and_time($timestamp, $is_gm_time = FALSE) {
// NOTE: event_block_upcoming() also has a friendly way of presenting dates
$date_function = ($is_gm_time) ? 'gmdate' : 'date';
  $today = date("m.d.Y");
  $yesterday = date("m.d.Y", strtotime("-1 day"));
  $in_date = $date_function("m.d.Y", $timestamp);
  $current_year = $date_function("Y");
  $now = time();
  
  $time_format = ($timestamp % 3600) ? 'g:i A' : 'g A';

  if ($in_date == $today) {
    $date = t('today at %time', array('%time' => $date_function($time_format, $timestamp)));
  }
  elseif ($in_date == $yesterday) {
    $date = t('yesterday at %time', array('%time' => $date_function($time_format, $timestamp)));
  }
  elseif (($timestamp < $now) && ($timestamp > strtotime("-7 days"))) {
    $date = t('last %week_day', array('%week_day' => $date_function("l @ $time_format", $timestamp)));
  }
  elseif (($timestamp > $now) && ($timestamp < strtotime("+7 days"))) {
    $date = t('next %week_day', array('%week_day' => $date_function("l @ $time_format", $timestamp)));
  }
  elseif($date_function("Y") == $date_function("Y", $timestamp)) {
    $date = t('on %week_day', array('%week_day' => $date_function("l, M j @ $time_format", $timestamp)));
  }
  else {
    $date = t('on %week_day', array('%week_day' => $date_function("l, M j, Y @ $time_format", $timestamp)));
  }
  return $date;
}

/*
 * Convert date to a friendly format
 */
function _whatsup_friendly_date($timestamp, $is_gm_time = FALSE) {
  $date_function = ($is_gm_time) ? 'gmdate' : 'date';
  
  $today = date("m.d.Y");
  $yesterday = date("m.d.Y", strtotime("-1 day"));
  $in_date = $date_function("m.d.Y", $timestamp);
  $now = time();

  if ($in_date == $today) {
    $date = t('today');
  }
  elseif ($in_date == $yesterday) {
    $date = t('yesterday');
  }
  elseif (($timestamp < $now) && ($timestamp > strtotime("-7 days"))) {
    $date = t('last %week_day', array('%week_day' => $date_function('l', $timestamp)));
  }
  elseif (($timestamp > $now) && ($timestamp < strtotime("+7 days"))) {
    $date = t('next %week_day', array('%week_day' => $date_function('l', $timestamp)));
  }
  else {
    $date = $date_function('l, M j, Y', $timestamp);
  }
  return $date;
}


/*
 * Call the specified What's Up extension.
 * 
 * @param $extension_number
 *   integer with the number of the extension to go to, or NULL in case the extension number is to be input by the user
 *
 * @return
 *   integer -1 in case of processing error, 0 in case of recoverable error,
 *   1 in case of success. In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_extension($extension_number = NULL) {
  global $wu_voip_server;
  global $wu_user_info;
  
eh_log("whatsup_extension()");
eh_log('wu_user_info: ' . serialize($wu_user_info));

  $selecting_extensions = FALSE;
  if (is_null($extension_number)) {
    $selecting_extensions = TRUE;
  }
  
  $get_user_input = TRUE;
  while ($selecting_extensions) {
  	if ($get_user_input) {
      //  ask for the desired extension
      vio_beep();
      $prompt = v("Please dial the desired extension or press star to go to the extension's directory");
      $no_input_msg = v('No input received. Please try again.');
      $input = vio_get_input($prompt, 3, $no_input_msg);
  	}
 eh_log("input: $input");
    
    switch($input) {
	  case -1: // Input error
	    eh_error('Error reading input from the user.');
	    return -1;
	
      case '': // user didn't type anything. go back
        eh_log('User did not press any extension.');
        return 0;
      
      case '*': // go to extension's directory
        $query_info = array();
        
        $type_choices = array();
        $type_choices[1] = v('For user extensions, press 1.');
        $type_choices[2] = v('For group extensions, press 2.');
        $type_choices[3] = v('For both user and group extensions, press 3.');
        $type_choices['#'] = v('To go back to the main menu, press the pound key.');
        $type_choice = vio_menu($type_choices);

        $dir_choices = array();        
        switch ($type_choice) {
          case 1: // user extensions
            $dir_choices[1] = v('To search user extensions by name, press 1.');
            $dir_choices[2] = v('To search user extensions by categories or areas of interest, press 2.');
            $query_info['get_user_extensions'] = TRUE;
            $query_info['get_node_extensions'] = FALSE;
            $no_extensions_msg = v("I am sorry, but there are no user extensions currently associated with the specified input. Please try again later.");
            break;
          case 2: // group extensions
            $dir_choices[1] = v('To search group extensions by name, press 1.');
            $dir_choices[2] = v('To search group extensions by categories or areas of interest, press 2.');
            $query_info['get_user_extensions'] = FALSE;
            $query_info['get_node_extensions'] = TRUE;
            $no_extensions_msg = v("I am sorry, but there are no group extensions currently associated with the specified input. Please try again later.");
            break;
          case 3: // both user and group extensions
            $dir_choices[1] = v('To search extensions by name, press 1.');
            $dir_choices[2] = v('To search extensions by categories or areas of interest, press 2.');
            $query_info['get_user_extensions'] = TRUE;
            $query_info['get_node_extensions'] = TRUE;
            $no_extensions_msg = v("I am sorry, but there are no extensions currently associated with the specified input. Please try again later.");
            break;
          case '#': // return to main menu
            return 1;
        }
      
        $dir_choices['#'] = v('To go back to the main menu, press the pound key.');
        
        $dir_choice = vio_menu($dir_choices);
        switch ($dir_choice) {
          case 1: // search by name
            $prompt = "Use the telephone keypad to type in the first letter of the desired extension's name.";
            $go_back_text = "To go back to the main menu, press the pound key.";
            $text = _whatsup_get_text($prompt, $go_back_text, 3, 1); 
eh_log("value returned from get_text: $text");
            if ($text === -1) {
              return -1;
            }
            else if (is_null($text) || ($text == '')) {
              return 1; // go back 
            }
            $query_info['name'] = $text;
            $query_info['is_active'] = 1;
            $query_info['is_listed'] = 1;
            $query_info['categories_or'] = NULL;
            $query_info['type_contents'] = NULL;
            $rc = _whatsup_directory_query_and_play($query_info, $no_extensions_msg);
            switch ($rc['error_code']) {
          	  default:
          	  case -1: // processing error
                $prompt = v("Error retrieving directory extensions. Please contact your system administrator.");
                vio_say($prompt);
                return -1;              
              case 0: // go back to main menu
                return 1;
              case 1: // keep going
                break;
              case 2: // go to specified extension
eh_log('Going to extension: ' . $rc['extension_number']);
                $input = $rc['extension_number'];
                $get_user_input = FALSE;
                break;
            }      
            break;

          case 2: // search by category
            $category_list = voip_get_category_list($wu_voip_server);
            $accept_none = FALSE;
            $accept_multiple = TRUE;
            $accept_any = TRUE;
            $any_categories_txt = "To select extensions from all categories, press %key.";
            $r = _whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $any_categories_txt);
            if (!$r->success) {
              $prompt = v("Error selecting categories from the server. Please contact your system administrator.");
              vio_say($prompt);
              return -1;
            }
            $query_info['name'] = NULL;
            $query_info['is_active'] = 1;
            $query_info['is_listed'] = 1;
            $query_info['categories_or'] = $r->selected_ids;
            $query_info['type_contents'] = NULL;
            $rc = _whatsup_directory_query_and_play($query_info, $no_extensions_msg);
            switch ($rc['error_code']) {
          	  default:
          	  case -1: // processing error
                $prompt = v("Error retrieving directory extensions. Please contact your system administrator.");
                vio_say($prompt);
                return -1;              
              case 0: // go back to main menu
                return 1;
              case 1: // keep going
                break;
              case 2: // go to specified extension
eh_log('Going to extension: ' . $rc['extension_number']);
                $input = $rc['extension_number'];
                $get_user_input = FALSE;
                break;
            }      
            break;

          case '#': // go back to main menu
            return 1;

          default:
            eh_log("Invalid option on directory selection: $dir_choice.");
            return -1;
        }
        break;
  
      default:
        $selecting_extensions = FALSE;
        $extension_number = $input;
        break;
    }
  }
//vio_say("You dialed extension $extension_number");

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $extension_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
  	eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = t("%extension_number is not a valid extension number.", array('%extension_number' => $extension_number));
    vio_say($prompt);
    return 0;
  }

  if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = t("I am sorry, but extension %extension_number does not exist.", array('%extension_number' => $extension_number));
    vio_say($prompt);
    return 0;
  }

  if ($result['error_code'] == VOIP_LACK_OF_PERMISSION) {
    eh_error("User not allowed to access specified extension: {$result['error_msg']}");
    $prompt = t("I am sorry, but you do not have the necessary permissions to access extension %extension_number.", array('%extension_number' => $extension_number));
    vio_say($prompt);
    return 0;
  }
  
  // process extension according to its type  
  $extension_info =& $result['extension_info'];

  $notes = 'extension uid: ' . (empty($extension_info['uid'])?'NULL':$extension_info['uid']) . ', type: ' . $extension_info['type_contents'] . ', name: ' . $extension_info['name'];
  voip_log($wu_voip_server, $wu_user_info, 'extension', 'called', $extension_number, $notes);

  
  // process user extensions
  if (!empty($extension_info['uid'])) {
  	$rc = _whatsup_user_extension($extension_info);
  }
  else {
    // process node extensions
    switch($extension_info['type_contents']) {
      case 'voip_group':
        $rc = _whatsup_group_extension($extension_info);
        break;
        
      case 'voip_event':
        $rc = _whatsup_event_extension($extension_info);
        break;
      
      default:
        eh_log('Processing error: unknown extension type: ' . $extension_info['type_contents'] . ' with node id: ' . $extension_info['nid_contents'] . '.');
        $rc = -1;
        break;
    }
  }
  
  return $rc;
}

function _whatsup_directory_query_and_play($query_info, $no_extensions_prompt = NULL) {
eh_log("Entering: _whatsup_directory_query_and_play: " . print_r($query_info, TRUE));

  global $wu_voip_server;
  global $wu_user_info;
  $rc = array();
  
  if (!$no_extensions_prompt) {
    $no_extensions_prompt = v("I am sorry, but there are no extensions under the specified criteria.");
  }
  
  $start = 0;
  $limit = 5;
  $retrieve_extension_batch = TRUE;
  $count_extensions = 0;
  while ($retrieve_extension_batch) {
//  	vio_say(v('Retrieving directory information from the server.'));
    $r = voip_get_extension_directory($wu_voip_server, $wu_user_info, $query_info, $start, $limit);
    if ($r['error_code'] != VOIP_SUCCESS) {
      $rc['error_code'] = -1; 
      return $rc;
    }

    $extensions =& $r['extensions'];
    $count_extensions += count($extensions);
      
    if (empty($extensions)) {
      if ($count_extensions == 0) {
        vio_say($no_extensions_prompt);
  	    $rc['error_code'] = 1;
  	    return $rc;
      }
      else {
        vio_say(v('There are no more extensions to be read.'));
        $retrieve_extension_batch = FALSE;
        continue;
      }
    }

    // Play the selected extensions to the caller
    $choices = array();
    $choices[1] = ''; // save the menu slot
    $choices[9] = v('To hear the next directory entry, press 9.');
    $choices['#'] = v('To go back to the main menu, press the pound key.');
  
    foreach ($extensions as $extension_info) {
      $extension_name = t('unknown extension');
      if (isset($extension_info['file_audio_name'])) {
        $extension_name = '*' . $extension_info['file_audio_name'];
      }
      else if (isset($extension_info['name'])) {
        $extension_name = $extension_info['name'];
      }
      $choices[1] = v('%name at extension %x. To go to this extension, press 1.', array('%name' => $extension_name, '%x' => $extension_info['number']));
  	  $choice = vio_menu($choices);
  	  switch ($choice) {
  	    case 1: // go to the specified extension
  	      $rc['error_code'] = 2;
  	      $rc['extension_number'] = $extension_info['number'];
  	      return $rc;
  	    case 9: // play the next directory entry
  	      break;
  	    case '#': // go back to the main menu
  	      $rc['error_code'] = 0;
  	      return $rc;
  	  }
    }
    
   // prepare to read next batch of extensions
   $start += $limit;

  }

  $rc['error_code'] = 1;
  return $rc;

}



/*
 * Process extension of a specific user
 */
function _whatsup_user_extension($extension_info) {
  global $wu_voip_server;
  global $wu_user_info;
  
  // welcome caller
  
  if (isset($extension_info['file_audio_greetings'])) {
    $basename = $extension_info['file_audio_greetings'];
    vio_say( "*$basename", '#');
  }
  else if (isset($extension_info['file_audio_name'])) {
    $basename = $extension_info['file_audio_name'];
    $prompt = v('Personal area for %user_name', array('%user_name' => "*$basename"));
    vio_say($prompt);
  }
  else {
    $prompt = v("Welcome to extension %n.", array('%n' => $extension_info['number']));
  	vio_say($prompt);
  }

  // check if extension has a voicemail account associated with it
  $rc = voip_voicemail_mailbox_get_info($wu_voip_server, $wu_user_info, $extension_info['number']);
  if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST)) {
    eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
    return -1;
  }
  $has_active_mailbox = (($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST) && ($rc['mailbox_info']['is_active']))?($rc['mailbox_info']['is_active']):FALSE;
  
  // now present the user extension menu 
  $stop = FALSE;
  while (!$stop) {

    vio_beep();
    vio_say('User extension menu');
    
    $choices = array();
    if ($has_active_mailbox) {
      $choices[1] = v('To leave a voicemail message, press 1.');
    }
    $choices[2] = v('To check audioblog entries posted by this user, press 2.');
    $choices[3] = v('To know more about this user, press 3.');
    $choices['#'] = v('To go back to the main menu, press the pound key.');
      	
    $choice = vio_menu($choices);
    switch($choice) {

      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting user extension menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = -1;
        $stop = TRUE;
        break;

      case 1:
        $destination_list = array($extension_info['number']);
        $rc2 = _whatsup_voicemail_message_record($destination_list, $extension_info['number']);
        switch ($rc2) {
          case -1:
          default:
            $rc = -1;
            $stop = TRUE;
            break;
          case 0: // go back to main menu
            $rc = 0;
            $stop = TRUE;
            break;
          case 1: // success: keep going
            break;
        } 
        break;

      case 2:
        if (isset($extension_info['file_audio_name'])) {
          $basename = $extension_info['file_audio_name'];
          $prompt = v('Retrieving audioblog entries posted by %b', array('%b' => "*$basename"));
          vio_say($prompt);
        }
        else {
          $prompt = v("Retrieving audioblog entries posted by extension %x user.", array('%x' => $extension_info['number']));
  	      vio_say($prompt);
        }
        $uid = $extension_info['uid'];
        $query_info = array();
	    $query_info['status'] = 1;
	    $query_info['uid'] = $uid;
	    $query_info['categories_or'] = NULL;
	    $query_info['categories_and'] = NULL;
	    $query_info['unread_entries_only'] = FALSE;
        $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries posted by this user currently available. Please try again later.");
        $r = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
        switch ($r) {
	      default:
	      case -1: // processing error
	        $prompt = v("Error retrieving audioblog entries. Please contact your system administrator.");
	        vio_say($prompt);
	        $rc = -1;
	        $stop = TRUE;
	        break;
	              
	      case 0: // cancel. go back to main menu
	        $rc = 0;
	        $stop = TRUE;
	        break;
	              
	      case 1: // keep going
	        break;
	    }
        break;

      case 3:
         if (isset($extension_info['file_audio_description'])) {
           $basename = $extension_info['file_audio_description'];
           vio_control_stream_file( $basename, '#');
         }
         else {
           $prompt = v("I'm sorry, but there is no audio description recorded for this user.");
  	       vio_say($prompt);
         }
         break;

      case '#':
        $rc = 1;
        $stop = TRUE;
        break;
    } // switch  
  } // while
  
  return $rc;
}

/*
 * Process extension of a specific group
 */
function _whatsup_group_extension($extension_info) {
  global $wu_voip_server;
  global $wu_user_info;
eh_log("group: " . print_r($extension_info, TRUE));  
  // determine the group's name
  if (isset($extension_info['file_audio_name'])) {
    $basename = $extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $extension_info['number']));
  }

  // welcome caller
  
  if (isset($extension_info['file_audio_greetings'])) {
    $basename = $extension_info['file_audio_greetings'];
    vio_control_stream_file( "*$basename", '#');
  }

  // check if extension has a voicemail account associated with it
  $rc = voip_voicemail_mailbox_get_info($wu_voip_server, $wu_user_info, $extension_info['number']);
  if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST)) {
    eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
    return -1;
  }
  $has_active_mailbox = (($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST) && ($rc['mailbox_info']['is_active']))?($rc['mailbox_info']['is_active']):FALSE;
  
  // now present the group extension menu 
  $stop = FALSE;
  while (!$stop) {
    // present group extension menu    
    vio_beep();
    vio_say(v('Extension menu for %group_name.', array('%group_name' => $group_name)));

    $choices = array();
    if ($has_active_mailbox) {
      $choices[1] = v('To leave a voicemail message to this group administrators, press 1.');
    }
    $choices[2] = v('To listen to audioblog entries from this group, press 2.');
// TODO: the following option should only show up if the group has upcoming events...
    $choices[3] = v('To check upcoming group events, press 3.');
    $choices[4] = v('To know more about this group, press 4.');
    $choices['*'] = v('To go to the group member area, press the star key.');
    $choices['#'] = v('To go back to the main menu, press the pound key.');
      	
    $choice = vio_menu($choices);
    switch($choice) {

      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting group extension menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = -1;
        $stop = TRUE;
        break;

      case 1:
        $destination_list = array($extension_info['number']);
        $rc2 = _whatsup_voicemail_message_record($destination_list, $extension_info['number']);
        switch ($rc2) {
          case -1:
          default:
            $rc = -1;
            $stop = TRUE;
            break;
          case 0: // go back to main menu
            $rc = 0;
            $stop = TRUE;
            break;
          case 1: // success: keep going
            break;
        } 
        break;

      case 2: // group audioblog entries
        // remind user to login
        $prompt_login_option = v('Only registered users have access to non-public audioblog entries. To loggin, press 1.');
        $prompt_other_option = v('To remain anonymous, press the pound key.');
        $r = _whatsup_check_login($prompt_login_option, $prompt_other_option);
        if ($r == -1) {
          $rc = -1;
	      $stop = TRUE;
	      break;
        }
        $query_info = array();
	    $query_info['status'] = 1;
	    $query_info['uid'] = NULL;
	    $query_info['categories_or'] = NULL;
	    $query_info['categories_and'] = NULL;
	    $query_info['unread_entries_only'] = FALSE;
	    $query_info['gid'] = $extension_info['nid_contents'];
        $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries posted for %group_name. Please try again later.", array('%group_name' => $group_name));
        $r = _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg);
        switch ($r) {
	      default:
	      case -1: // processing error
	        vio_say(v("Error retrieving audioblog entries. Please contact your system administrator."));
	        $rc = -1;
	        $stop = TRUE;
	        break;
	              
	      case 0: // cancel. go back to main menu
	        $rc = 0;
	        $stop = TRUE;
	        break;
	              
	      case 1: // keep going
	        break;
	    }
        break;


      case 3: // upcoming group events
        // remind user to login
        $prompt_login_option = v('Only registered users have access to non-public events. To loggin, press 1.');
        $prompt_other_option = v('To remain anonymous, press the pound key.');
        $r = _whatsup_check_login($prompt_login_option, $prompt_other_option);
        if ($r == -1) {
          $rc = -1;
	      $stop = TRUE;
	      break;
        }
        $timestamp = time();
        $start = array();
        $start['year'] = date('Y', $timestamp);
        $start['month'] = date('n', $timestamp);
        $start['day'] = date('j', $timestamp);
        $start['hour'] = date('G', $timestamp);
        $start['minute'] = date('i', $timestamp);
        $start['second'] = date('s');
        $query_info['start'] = $start;
        $query_info['end'] = NULL;
        $query_info['limit'] = NULL;
        $query_info['terms'] = NULL;
        $query_info['organizer_extension_number'] = $extension_info['number'];
        $query_info['gid'] = $extension_info['nid_contents'];
        $prompt_no_events = v('I am sorry, but there are no events currently planned for %group_name. Please try again later.', array('%group_name' => $group_name));
        $prompt_end_of_events = v('End of event listing.');
        $prompt_description = v('Upcoming events for %group_name', array('%group_name' => $group_name));
        $txt_calling_extension_name = t('the group menu.');
        $r = _whatsup_event_query_and_play($wu_voip_server, $wu_user_info, $query_info, $prompt_no_events, $prompt_end_of_events, $prompt_description, $txt_calling_extension_name);
        switch ($r) {
	      default:
	      case WU_PROCESSING_FAILURE: // processing error
             vio_say(v("Processing error. Please contact your system administrator."));
	        $rc = -1;
	        $stop = TRUE;
	        break;
	              
	      case WU_GO_TO_MAIN_MENU: // cancel. go back to main menu
	        $rc = 0;
	        $stop = TRUE;
	        break;
	              
          case WU_GO_BACK: 
          case WU_SUCCESS: 
	        break;
	    }
        break;
              
      case 4: // group description
         if (isset($extension_info['file_audio_description'])) {
           $basename = $extension_info['file_audio_description'];
           vio_control_stream_file( $basename, '#');
         }
         else {
           $prompt = v("I'm sorry, but there is no audio descriptions recorded for %group_name.", array('%group_name' => $group_name));
  	       vio_say($prompt);
         }
         break;

      case '*':
        $rc2 = _whatsup_group_member_area($extension_info);
        switch ($rc2) {
          case -1:
          default:
            $rc = -1;
            $stop = TRUE;
            break;
          case 1: // success: keep going
            break;
          case 2: // group got deleted
            $rc = 1;
            $stop = TRUE;
            break;
        } 
        break;

      case '#':
        $rc = 1;
        $stop = TRUE;
        break;
    } // switch  
  } // while
  
  return $rc;
}

/*
 * Allows users to contact other group members and configure the way the group is presented.
 *
 * @return
 *   integer -1 in case of processing failure, 1 in case of success, 2 in case the group gets deleted
 *   In case of failure, check $eh_error_msg() for explanation.
 */
function _whatsup_group_member_area($extension_info) {
eh_log("whatsup_group_member_area()");

  global $wu_voip_server;
  global $wu_user_info;
  
  // determine the group's name
  if (isset($extension_info['file_audio_name'])) {
    $basename = $extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $extension_info['number']));
  }
     
  // Make sure user is logged in
  $prompt_login_option = v('Only subscribed users are allowed to access this area. To loggin, press 1.');
  $prompt_other_option = v('To go back to %group_name extension, press the pound key.', array('%group_name' => $group_name));
  $r = _whatsup_check_login($prompt_login_option, $prompt_other_option);
  if ($r != 1) {
    return ($r == -1)? -1 : 1;
  }

  
  // retrieve current user's extension info
  $r = voip_extension_get_info($wu_voip_server, $wu_user_info, $wu_user_info['extension_number']);
  if ($r['error_code'] != VOIP_SUCCESS) {
    eh_error_msg('Error retrieving user extension: ' . $r['error_msg']);
    vio_say(v('Processing error retrieving user extension.'));
    return -1;
  }
  $user_extension_info = $r['extension_info'];

  // make sure the caller is a member of the group
  $is_member = FALSE;
  $subscribers = array();
  $r = voip_group_get_subscribers($wu_voip_server, $wu_user_info, $extension_info['nid_contents'], 1, 0);
  switch($r['error_code']) {
    case VOIP_SUCCESS:
      $subscribers = $r['subscribers'];
      break;
    case VOIP_LACK_OF_PERMISSION:
      // in general, only group members are allowed to get the list of subscribers
      break;
    default:
      eh_error_msg('Error retrieving list of group members: ' . serialize($r));
      return -1;
  }

  $is_member = in_array($user_extension_info['number'], array_keys($subscribers));

  $is_admin = FALSE;
  if ($is_member) {
    $is_admin = $subscribers[$user_extension_info['number']]['is_admin'];
  }
  
  if (!$is_member) {
eh_log("user is not a member of the group");
  	// check the group's subscription mode
  	$r = voip_group_get_subscription_mode($wu_voip_server, $extension_info['nid_contents']);
  	if ($r['error_code'] != VOIP_SUCCESS) {
      eh_error_msg('Error retrieving group subscription mode: ' . serialize($r));
      vio_say(v('Processing error. Please contact your system administrator.'));
      return -1;
  	}
  	switch ($r['subscription_mode'] ) {
  	  case 'closed':
  	  case 'invitation only':
  	    vio_say(v("I am sorry, but the member area of %group_name is only accessible to existing member's of the group'.", array('%group_name' => $group_name)));
  	    return 1;
  	  default:  
        break;
  	}
  	vio_say(v("I am sorry, but you are not a member of %group_name.", array('%group_name' => $group_name)));
eh_log('present subscription options');
    $choices = array();
    $choices[1] = v('To subscribe to this group, press 1.');
    $choices['#'] = v('To go back to the group extension menu, press the pound key.', array('%group_name' => $group_name));
eh_log('subscription options: ' . print_r($choices, TRUE));
    $choice = vio_menu($choices);
eh_log('choice made: ' . $choice);
    switch($choice) {

      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting group subscription menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        return -1;
        
      case '#':
//        vio_say(v('Going back to group extension.'));
        return 1;
        
      case 1: // subscribe to group
        $r = voip_group_subscribe($wu_voip_server, $wu_user_info, $extension_info['nid_contents'], $user_extension_info['uid']);
        switch ($r['error_code']) {
          case VOIP_SUCCESS:
            switch ($r['subscription_status']) {
              case 'approval':
                vio_say(v('Your subscription request is awaiting approval by a group administrator.'));
                return 1;
              case 'rejected':
                vio_say(v('Your subscription request was rejected. Only group administrators can add users to this group.'));
                return 1;
              case 'subscribed':
                vio_say(v('Your subscription has been approved.'));
                // NOTE: we are assuming that newly-subscribed users are not administrators
                break;
            }
            break;
            
          case VOIP_LACK_OF_PERMISSION:
            vio_say(v('You do not have the necessary permissions to subscribe to this group. Are you sure this group has open subscriptions?'));
            eh_error_msg($r['error_msg']);
            return 1;
             
          default:
            vio_say(v('Processing error. Please contact your system\'s administrator.'));
            eh_error_msg($r['error_msg']);
            return -1;
        }
        break;
    }
  }
  
  //
  // from this point on, we assume the user is a member of the group
  //
  
  $has_active_mailbox = FALSE;
    
  $stop = FALSE;
  while (!$stop) {
    // Welcome caller to the group member's area
    vio_beep();
    vio_say(v("Member's area for %group_name.", (array('%group_name' => $group_name))));

    if ($is_admin) {
      // Make sure group has a mailbox and, if so, present the number of new messages in the mailbox  
      $rc = voip_voicemail_mailbox_get_info($wu_voip_server, $wu_user_info, $extension_info['number']);
      if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST)) {
        eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
        return -1;
      }
      if ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST) {
        $mailbox_info = $rc['mailbox_info'];
        if ($mailbox_info['is_active']) {
          $has_active_mailbox = TRUE;
          $rc = voip_voicemail_mailbox_get_number_of_messages($wu_voip_server, $wu_user_info, $extension_info['number']);
          if ($rc['error_code'] != VOIP_SUCCESS) {
            eh_error('Error retrieving number of messages from mailbox: ' . $rc['error_msg']);
            return -1;
          }  
          $new_msg_count = $rc['count_new_messages'];
          $new_msg_text = ($new_msg_count <= 0)?'':(($new_msg_count == 1)?t('There is 1 new message'):t('There are %count new messages  ', array('%count' => $new_msg_count)));
          if (!empty($new_msg_text)){
            $prompt = v('%new_text in the group voicemail box.', array('%new_text' => $new_msg_text));
            vio_say($prompt);
          }
        }
      }
    }
      
    // now present the member's area menu
    
    $choices = array();
    if ($is_admin && $has_active_mailbox) {
      $choices[1] = v('To check the group voicemail messages, press 1.');
    }
    $choices[2] = v('To leave a voicemail message to other members of the group, press 2.');
    $choices[3] = v('To record a group audioblog entry, press 3.');
    if ($is_admin) {
      $choices[4] = v('To create and manage community events, press 4.');
      $choices[5] = v('To change group configuration settings, press 5.');
      $choices[6] = v('For membership management, press 6.');
      $choices[7] = v('To delete this group, press 7.');
    }
    $choices['#'] = v('To go back to the group\'s extension menu, press the pound key.');
      	
    $choice = vio_menu($choices);
    switch($choice) {
      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting group menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = -1;
        $stop = TRUE;
        break;

     case 1:
        $rc2 = _whatsup_voicemail_check_messages($wu_voip_server, $extension_info['number']);
        switch ($rc2) {
          case -1:
          default:
            $rc = -1;
            $stop = TRUE;
            break;
          case 2: // go back to the group's main menu
            $rc = 1;
            $stop = TRUE;
            break;
          case 1: // success: keep going
            break;
        } 
        break;
        
      case 2: // send voicemail message to all active members of the group
        $destination_list = array();
        foreach ($subscribers as $extension_number => $status) {
          if ($status['is_active']) {
            $destination_list[] = $extension_number;
          }
        }
        $r = _whatsup_voicemail_message_record($destination_list, $extension_info['number']);
        if ($r == 1) {
          // log statistics
          $notes = "name: " . $extension_info['name'];
          voip_log($wu_voip_server, $wu_user_info, 'voicemail', 'sent group voicemail to', $extension_info['number'], $notes);
        }
        else if ($r == -1) {
          $rc = -1;
          $stop = TRUE;
        }
        break;

      case 3: // record a new group / community audioblog entry
        $rc = _whatsup_audioblog_create($extension_info, $is_admin);
        if ($rc == -1) {
          $prompt = v("Processing error. Please contact your system administrator.");
           vio_say($prompt);
           return -1;
        }
        break;
        
      case 4: // manage community events
        $calling_extension_name = t("group member's area");
        $rc = _whatsup_event_management_area($extension_info, $is_admin, $calling_extension_name);
        if ($rc == -1) {
          return -1;
        }
        break;
 
      case 5: 
        vio_say('Group configuration menu');
        $choices2 = array();
        $choices2[1] = v('To record your group\'s name, press 1');
        $choices2[2] = v('To record a welcome message for your group\'s extension, press 2');
        $choices2[3] = v('To record an audio description about your group, press 3');
        $choices2[4] = v('To change the categories associated with this group, press 4.');
        $choices2['#'] = v('To go back to the group\'s member area, press the pound key');
        $choice2 = vio_menu($choices2);
        switch($choice2) {
          case 1:
           $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_name');
            if ($r == -1) {
              $rc = -1;
              $stop = TRUE;
            }
           
            break;
            
          case 2:
            $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_greetings');
            if ($r == -1) {
              $rc = -1;
              $stop = TRUE;
            }
            break;
            
          case 3:
            $r = _whatsup_extension_record_audio_field($extension_info['number'], 'audio_description');
            if ($r == -1) {
              $rc = -1;
              $stop = TRUE;
            }
            break;
            
          case 4: // change extension categories
            $category_list = voip_get_category_list($wu_voip_server);
            $accept_none = TRUE;
            $accept_multiple = TRUE;
            $accept_any = FALSE;
            $any_categories_txt = NULL;
            $r = _whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $any_categories_txt);
            if (!$r->success) {
              $prompt = v("Error selecting categories from the server. Please contact your system administrator.");
              vio_say($prompt);
              $rc = -1;
              $stop = TRUE;
            }
            $category_ids =& $r->selected_ids;
            $r = voip_extension_set_categories($wu_voip_server, $wu_user_info, $extension_info['number'], $category_ids);
            if ($r['error_code'] != VOIP_SUCCESS) {
              vio_say(v('Processing error. Please contact your system administrator.'));
              $rc = -1;
              $stop = TRUE;
            }
            vio_say(v('The categories associated with your group have been succesfully updated.'));
            break;
            
          case '#':
            break;
            
          default:
            eh_error("Error selecting personal settings menu options (choice: $choice)");
            $prompt = v('Processing error. Please contact your system administrator.');
            vio_say($prompt);
            $rc = -1;
            $stop = TRUE;
            break;
        }
        break;

      case 6: 
        $r = _whatsup_group_member_management_area($extension_info);
        if ($r == -1) {
          $rc = -1;
          $stop = TRUE;
        }
        break;
        
      case 7: // delete group
//-----------------------------------
        $choices_del = array();
        $choices_del[1] = v('Are you sure you want to delete %group_name? If so, press 1.', array('%group_name' => $group_name));
        $choices_del['#'] = v('To cancel this operation, press the pound key.');
        $choice_del = vio_menu($choices_del);
        switch ($choice_del) {
          case -1:
            eh_error('Error selecting delete group options');
            $rc = -1;
            $stop = TRUE;
            break;

          case '#': 
            vio_say(v("Operation cancelled."));
            break;

          case 1:
// TODO: shall delete group also delete the posts published for that group?
            $r = voip_group_delete_group($wu_voip_server, $wu_user_info, $extension_info);
            switch ($r['error_code']) {
              default: 
                eh_error('Processing error deleting group: ' . $r['error_msg']);
                $rc = -1;
                $stop = TRUE;
                break;
              case VOIP_SUCCESS:
                vio_say(v('Group successfully deleted.'));
                return 2;
              case VOIP_LACK_OF_PERMISSION:
                $prompt = v("I am sorry, but you do not have the necessary permissions to delete this group.");
                vio_say($prompt);
                break;
            }
            break;
        }
        break;
//-----------------------------------

      case '#':
        $rc = 1;
        $stop = TRUE;
        break;
    } // switch  
  } // while
  
  return $rc;
}


/*
 * Return -1 in case of processing failure, 0 to return to main menu, 1 in case of success
 *
 */
function _whatsup_group_member_management_area($extension_info) {
  global $wu_user_info;
  global $wu_voip_server;
  
  // determine the group's name
  if (isset($extension_info['file_audio_name'])) {
    $basename = $extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $extension_info['number']));
  }
  
  $update_members_list = TRUE;
  $stop = FALSE;
  while (!$stop) {
    // Welcome caller to the group's member management area
    $prompt = v('Member management area for %group_name', array('%group_name' => $group_name));

    if ($update_members_list) {
      // retrieve list of all group subscribers (including the pending ones)
      vio_say(v('Please wait a couple of seconds while I download group membership information.'));
      $subscribers = array();
      $rc = voip_group_get_subscribers($wu_voip_server, $wu_user_info, $extension_info['nid_contents'], 0, 0);
eh_log('get_subscribers result: ' . print_r($rc, TRUE));
      switch($rc['error_code']) {
        case VOIP_SUCCESS:
          foreach($rc['subscribers'] as $extension_number => $subscriber) {
  	        $r = voip_extension_get_info($wu_voip_server, $wu_user_info, $extension_number);
            if ($r['error_code'] == VOIP_SUCCESS) {
  	          $rc['subscribers'][$extension_number]['extension_info'] = $r['extension_info'];
  	        }
  	        else {
              eh_error_msg('Error retrieving list of group members: ' . serialize($r));
              return -1;
   	        }
          }
          $subscribers =& $rc['subscribers'];
          $update_members_list = FALSE;
          break;
        case VOIP_LACK_OF_PERMISSION:
          // in general, only group members are allowed to get the list of subscribers
          // in this case, since only group administrators are allowed here, this is a processing error
        default:
          eh_error_msg('Error retrieving list of group members: ' . serialize($r));
          return -1;
      }
    }

    // sort subscribers according to their group status
    $active = array();
    $pending = array();
    $admins = array();
    foreach ($subscribers as $extension_number => $subscriber) {
      if ($subscriber['is_active']) {
      	$active[$extension_number] =& $subscribers[$extension_number]['extension_info'];
      	if ($subscriber['is_admin']) {
      	  $admins[$extension_number] =& $subscribers[$extension_number]['extension_info'];
      	}
      }
      else {
        $pending[$extension_number] =& $subscribers[$extension_number]['extension_info'];
      } 
    }
/**
eh_log("group " . $extension_info['number'] . ' pending: ' . implode(', ', array_keys($pending)));
eh_log("group " . $extension_info['number'] . ' active: ' . implode(', ', array_keys($active)));
eh_log("group " . $extension_info['number'] . ' admins: ' . implode(', ', array_keys($admins)));
**/
    // present the member management menu
    vio_beep();
    vio_say(v('Member management menu for %group_name', array('%group_name' => $group_name)));
    
    if ($pending) {
      $count = count($pending);
      $prompt = ($count == 1)?v('%group_name has 1 pending subscriber waiting for your approval.', array('%group_name' => $group_name))
                                 :v('%group_name has %n pending subscribers waiting for your approval', array('%group_name' => $group_name, '%n' => $count));
      vio_say($prompt);
    }
    
    $choices = array();
    $choices[1] = v("To check the group member's directory, press 1.");
    $choices[2] = v('To add a member to the group, press 2.');
    $choices[3] = v('To remove a member from the group, press 3.');
    $choices[4] = v('To check the group administrator\'s directory, press 4.');
    $choices[5] = v('To add an administrator to the group, press 5.');
    $choices[6] = v('To remove an administrator from the group, press 6.');
   if ($pending) {
      $choices[7] = v('To manage pending subscriptions, press 7.');
    }
    $choices['#'] = v('To go back to the group\'s member area, press the pound key.');
    $choice = vio_menu($choices);
    switch($choice) {
      case 1: // member directory
        // sort subscriber's list by their extension name and number
        uasort($active, '_whatsup_sort_extensions');

        $count = count($active);
        $prompt = ($count == 1)?v('%group_name has only 1 member: ', array('%group_name' => $group_name))
                               :v('%group_name has %n members:', array('%group_name' => $group_name, '%n' => $count));
        vio_say($prompt);
        $counter = 0;
        $prompt = array();
        foreach($active as $subscriber_extension) {
          $counter++;
          $subscriber_name = '';
          if (!empty($subscriber_extension['file_audio_name'])) {
            $subscriber_name = '*' . $subscriber_extension['file_audio_name'];
          }
          else if (!empty($subscriber_extension['name'])) {
            $subscriber_name = $subscriber_extension['name'];
          }
          else {
          	$subscriber_name = t('An anonymous user ');
          }
          if (($count > 1) && ($counter == $count)) {
            $new_prompt = v(' and ');
            $prompt = array_merge($prompt, $new_prompt);
          }
          $new_prompt = v('%n with extension %x. ', array('%n' => $subscriber_name, '%x' => $subscriber_extension['number']));
eh_log('new_prompt: ' . print_r($new_prompt, TRUE));
eh_log('prompt: ' . print_r($prompt, TRUE));
  	      $prompt = array_merge($prompt, $new_prompt);
        }
eh_log('final prompt: ' . print_r($prompt, TRUE));
        if ($prompt) {
          vio_say($prompt, '#');
        }
       break;
            
      case 2:
        $r = _whatsup_group_subscribe($extension_info, $subscribers);
        switch($r) {
          case 1:
            break;
          case 2:
            $update_members_list = TRUE;
            break;
          default:
            return $r;  
        }
        break;
            
      case 3:
        $r = _whatsup_group_unsubscribe($extension_info, $subscribers);
        switch($r) {
          case 1:
            break;
          case 2:
            $update_members_list = TRUE;
          default:
            return $r;  
        }
        break;
            
      case 4: // admin directory
        // sort subscriber's list by their extension name and number
        uasort($admins, '_whatsup_sort_extensions');

        $count = count($admins);
        $prompt = ($count == 1)?v('%group_name has only 1 administrator: ', array('%group_name' => $group_name))
                               :v('%group_name has %n administrators:', array('%group_name' => $group_name, '%n' => $count));
        vio_say($prompt);
        $counter = 0;
        $prompt = array();
        foreach($admins as $subscriber_extension) {
          $counter++;
          $subscriber_name = '';
          if (!empty($subscriber_extension['file_audio_name'])) {
            $subscriber_name = '*' . $subscriber_extension['file_audio_name'];
          }
          else if (!empty($subscriber_extension['name'])) {
            $subscriber_name = $subscriber_extension['name'];
          }
          else {
          	$subscriber_name = t('An anonymous user ');
          }
          if (($count > 1) && ($counter == $count)) {
            $new_prompt = v(' and ');
            $prompt = array_merge($prompt, $new_prompt);
          }
          $new_prompt = v('%n with extension %x. ', array('%n' => $subscriber_name, '%x' => $subscriber_extension['number']));
 	      $prompt = array_merge($prompt, $new_prompt);
        }
        if ($prompt) {
          vio_say($prompt, '#');
        }
        break;
            
      case 5:
        $r = _whatsup_group_create_admin($extension_info, $subscribers, $admins);
        switch($r) {
          case 1:
            break;
          case 2:
            $update_members_list = TRUE;
            break;
          default:
            return $r;  
        }
        break;
            
      case 6:
        $r = _whatsup_group_delete_admin($extension_info, $admins);
        switch($r) {
          case 1:
            break;
          case 2:
            $update_members_list = TRUE;
            break;
          default:
            return $r;  
        }
        break;
            
      case 7:
        $r = _whatsup_group_manage_pending_subscriptions($extension_info, $pending);
        switch($r) {
          case 1:
            break;
          case 2:
            $update_members_list = TRUE;
            break;
          default:
            return $r;  
        }
        break;
            
      case '#':
        $stop = TRUE;
        $rc = 1;
        break;
            
      default:
        eh_error("Error selecting member management menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = -1;
        $stop = TRUE;
        break;
          
    }        	
  }
  return $rc;
}

/*
 * Sort 2 extensions based on their names and numbers
 */
function _whatsup_sort_extensions($a, $b) {
  $a_name = empty($a['name'])?'':$a['name'];
  $b_name = empty($b['name'])?'':$b['name'];
  
  $cmp = strcmp($a_name, $b_name);
  
  if (!$cmp) {
  	$cmp = ($a['number'] < $b['number'])? -1 : 1; 
  }
  
  return $cmp;
}

/*
 * Play the specified voicemail message and handle advanced options associated with it.
 *
 * @param $voicemail_server
 * URL of the xmlrpc.php of the site where the message is to be stored
 * 
 * @param $msg_info
 * array with metadata about the message to be played
 *
 * @return
 *   integer. -1 in case of processing failure, 1 in case of success, 
 *   2 to go back to personal area, 3 to go back to main menu, 4 to skip to next message.
 */
function _whatsup_voicemail_message_play($voicemail_server, $msg_info) {

  global $wu_user_info;
  
  $char_next = '9';
  $char_stop = '#';
  $escape_digits = " $char_next $char_stop";
  
  
  $choices = array();
  $choices[1] = v('To hear this message again, press 1');
  $choices[2] = v('To save it, press 2');
  $choices[3] = v('To erase it, press 3');
  $choices[4] = v('To hear envelope information, press 4');
  $choices['*'] = v('To go back to your personal area, press the star key');
  $choices['#'] = v('To go back to the main menu, press the pound key');
  
  // download message contents from the server 
  $basename_wav = asa_get_audio_file($voicemail_server, $wu_user_info, array('nid' => $msg_info['nid']));
  if (($basename_wav == -1) || ($basename_wav === 0)) {
    eh_error('Processing error retrieving voicemail message contents: ' . eh_error_msg());
    vio_say(v("Processing error. Please contact your system administrator."));
    return -1;
  }

  // play the message to the user
  $rc = vio_control_stream_file($basename_wav, $escape_digits);

  if (!$rc) {
eh_log("Error playing voicemail message audio file: " . eh_error_msg());
    vio_say("Processing error playing voicemail message.  Please contact your system administrator.");
    return -1;
  }
    
  // check if the user pressed the key to skip to next message
  switch ($rc['key_pressed']) {
    case $char_next: // skip to next message
      $rc = 4;
      return $rc;
  }  

  // now handle advanced options
  $stop = FALSE;
  while (!$stop) {    

    // present menu options for the message
    $choice = vio_menu($choices);
    switch($choice) {
      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting voicemail message menu options (choice: $choice)");
        $rc = -1;
        $stop = TRUE;;
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        break;
    
      case 1: // play the message again
        $rc = vio_control_stream_file($basename_wav, $escape_digits);
        if (!$rc) {
          eh_log("Error playing voicemail message audio file: " . eh_error_msg());
          vio_say("Processing error playing voicemail message.  Please contact your system administrator.");
          return -1;
        }
        // check if the user pressed the key to skip to next message
        switch ($rc['key_pressed']) {
          case $char_next: // skip to next message
            $rc = 4;
            return $rc;
        }  
        break;

      case 2: // save the message, i.e. update access statistics associated with it
        vio_say('save the message'); // update last modified...
        $rc2 = voip_voicemail_msg_save($voicemail_server, $wu_user_info, $msg_info);
eh_log('value returned by msg_save: ' . serialize($rc2));
        if ($rc2['error_code'] != VOIP_SUCCESS) {
          $rc = -1;
        }
        else { 
          $rc = 1;
        }
        $stop = TRUE;
eh_log("value of rc: $rc");
        break;
        
      case 3: // erase the message
        $choices2 = array();
        $choices2[1] = v('Are you sure you want to delete this message? If so, press 1.');
        $choices2['#'] = v('To cancel this operation, press the pound key.');
        $choice2 = vio_menu($choices2);
        switch ($choice2) {
          case -1:
            eh_error('Error selecting erase voicemail message options');
            return -1;

          case '#': 
            $prompt = v("Operation cancelled.");
            vio_say($prompt);
            continue;

          case 1:
            $rc2 = voip_voicemail_msg_delete($voicemail_server, $wu_user_info, $msg_info);
            if (($rc2['error_code'] != VOIP_SUCCESS) && ($rc2['error_code'] != VOIP_LACK_OF_PERMISSION)) {
              $prompt = v('Processing error deleting voicemail message. Please contact your system administrator.');
              vio_say($prompt);
              eh_error('Error deleting voicemail message: ' . $rc2['error_msg']);
              return -1;
            }
            if ($rc2['error_code'] == VOIP_LACK_OF_PERMISSION) {
              $prompt = v('I am sorry, but you do not have permissions to delete this message.');
              vio_say($prompt);
              eh_error('Error deleting voicemail message: ' . $rc2['error_msg']);
              return 1; // return success
            }    
            $prompt = v("Message deleted.");
            vio_say($prompt);
            $stop = TRUE;
            $rc = 1;       
        }
        break;
        
      case 4: // play message envelope
        if ($msg_info['caller_extension']) {
          $prompt = v('Message received %date from extension %x.', array( '%date' => _whatsup_friendly_date_and_time($msg_info['received']), '%x' => $msg_info['caller_extension']));
        }
        else {
          $prompt = v('Message received %date.', array( '%date' => _whatsup_friendly_date_and_time($msg_info['received'])));
        }
        vio_say($prompt); 
        break; // continue with the menu options
        
      case '*': // go back to personal area
        vio_say(v('go back to personal area'));
        $rc = 2;
        $stop = TRUE;
        break;
        
      case '#': // go back to main menu
        vio_say(v('go back to main menu'));
        $rc = 3;
        $stop = TRUE;
        break;
    }
  }

  return $rc;   
}

/*
 * Subscribe a user into the specified group
 * 
 * @param $group_extension_info
 * 
 * @param $subscribers
 *   array of the extension_info for each member of the group. indexed by extension number 
 *
 * @return
 *   integer. -1 in case of processing failure, 0 in case no user input is received, 1 in case of success without changes in the group members, 2 in case of success with changes in the group members.
 *   In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_group_subscribe($group_extension_info, $subscribers) {
   global $wu_voip_server;
   global $wu_user_info;
   
  // determine the group's name
  if (!empty($group_extension_info['file_audio_name'])) {
    $basename = $group_extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $group_extension_info['number']));
  }
  
  // ask for the new member's extension number
  $prompt = v('Please dial the extension of the new group member. When done, press the pound key.');
  $no_input_msg = v('No input received. Please try again');
  $input = vio_get_input($prompt, 3, $no_input_msg);
eh_log("input: $input");
    
  switch($input) {
    case -1: // Input error
      eh_error('Error reading input from the user.');
      return -1;
	
    case '': // user didn't type anything. go back
      eh_log('User did not press any extension number.');
      return 0;

    default:
      $new_member_number = $input;
      break;
  }

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $new_member_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
    eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  else if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but %n is not a valid extension number.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but extension %n does not exist.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if (empty($result['extension_info']['uid'])) {
    eh_error("Given extension number is not associated with a user extension: " . serialize($result));
    $prompt = v("I am sorry, but extension %n is not a user extension.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  // check if user is not already a member of the group
  $user_extension_info =& $result['extension_info'];
  $member_name = (empty($user_extension_info['file_audio_name']))
                 ? t('the user from extension %n', array('%n' => $new_member_number))
                 : '*' . $user_extension_info['file_audio_name'];
  if (in_array($new_member_number, array_keys($subscribers))) {
    $prompt = v("I am sorry, but %name is already a member of %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 1;
  }
  // add member to the group        
  $r = voip_group_subscribe($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $user_extension_info['uid']);
  if (($r['error_code'] == VOIP_SUCCESS) && ($r['subscription_status'] == 'subscribed')) {
    $prompt = v("%name successfully subscribed into %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    // log statistics
    $notes = "name: " . $group_extension_info['name'];
    voip_log($wu_voip_server, $wu_user_info, 'groups', 'subscribed to', $group_extension_info['number'], $notes);
    
    return 2;
  }
  else {
  	eh_error('Error subscribing user: ' . (isset($r['error_msg'])?$r['error_msg']:'no error message'));
    $prompt = v("Processing error subscribing %name into %group_name. Please contact your system administrator.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return -1;
  }
}
 
/*
 * Unsubscribe a user from the specified group
 * 
 * @param $group_extension_info
 * 
 * @param $subscribers
 *   array of the extension_info for each member of the group. indexed by extension number 
 *
 * @return
 *   integer. -1 in case of processing failure, 0 to return to the main menu, or 1 in case of success without changes in the group members, or 2 in case of success with changes in the group members.
 *   In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_group_unsubscribe($group_extension_info, $subscribers) {
   global $wu_voip_server;
   global $wu_user_info;
   
  // determine the group's name
  if (!empty($group_extension_info['file_audio_name'])) {
    $basename = $group_extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $group_extension_info['number']));
  }
  
  // ask for the extension number of the member to be unsubscribed
  $prompt = v('Please dial the extension of the member that you want to unsubscribe. When done, press the pound key.');
  $no_input_msg = v('No input received. Please try again');
  $input = vio_get_input($prompt, 3, $no_input_msg);
eh_log("input: $input");
    
  switch($input) {
    case -1: // Input error
      eh_error('Error reading input from the user.');
      return -1;
	
    case '': // user didn't type anything. go back
      eh_log('User did not press any extension number.');
      return 0;

    default:
      $new_member_number = $input;
      break;
  }

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $new_member_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
    eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  else if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but %n is not a valid extension number.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but extension %n does not exist.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if (empty($result['extension_info']['uid'])) {
    eh_error("Given extension number is not associated with a user extension: " . serialize($result));
    $prompt = v("I am sorry, but extension %n is not a user extension.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  // check if user really a member of the group
  $user_extension_info = $result['extension_info'];
  $member_name = (empty($user_extension_info['file_audio_name']))
                 ? t('the user from extension %n', array('%n' => $new_member_number))
                 : '*' . $user_extension_info['file_audio_name'];
  if (!in_array($new_member_number, array_keys($subscribers))) {
    $prompt = v("I am sorry, but %member_name is not a member of %group_name.", array('%member_name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 1;
  }
  // confirm user deletion
  $choices_del = array();
  $choices_del[1] = v('Are you sure you want to remove %member_name? If so, press 1.', array('%member_name' => $member_name));
  $choices_del['#'] = v('To cancel this operation, press the pound key.');
  $choice_del = vio_menu($choices_del);
  switch ($choice_del) {
    case -1:
      eh_error('Error selecting delete group member options');
      vio_say(v('Processing error deleting member from the group.  Please contact your system administrator.'));
      return -1;
    case '#': 
      vio_say(v("Operation cancelled."));
      return 1;
  }
  
  // remove member from the group     
  $r = voip_group_unsubscribe($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $user_extension_info['uid']);
  if (($r['error_code'] == VOIP_SUCCESS)) {
    $prompt = v("%name successfully unsubscribed from %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    // log statistics
    $notes = "name: " . $group_extension_info['name'];
    voip_log($wu_voip_server, $wu_user_info, 'groups', 'unsubscribed from', $group_extension_info['number'], $notes);
    return 2;
  }
  else {
  	eh_error('Error unsubscribing user: ' . (isset($r['error_msg'])?$r['error_msg']:'no error message'));
    $prompt = v("Processing error unsubscribing %name from %group_name. Please contact your system administrator.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return -1;
  }
}
 
/*
 * Promote user to administrator of the specified group
 * 
 * @param $group_extension_info
 * 
 * @param $subscribers
 *   array of the extension_info for each member of the group. indexed by extension number 
 *
 * @param $admins
 *   array of the extension_info for each administrator of the group. indexed by extension number 
 *
 * @return
 *   integer. -1 :: processing failure; set eh_error_msg() with the error description
 *            0 :: no user input received
 *            1 :: success without membership changes.
 *            2 :: success with membership changes.
 */
function _whatsup_group_create_admin($group_extension_info, $subscribers, $admins) {
   global $wu_voip_server;
   global $wu_user_info;
   
  // determine the group's name
  if (!empty($group_extension_info['file_audio_name'])) {
    $basename = $group_extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $group_extension_info['number']));
  }
  
  // ask for the extension number of the new administrator
  $prompt = v('Please dial the extension of the new administrator. When done, press the pound key.');
  $no_input_msg = v('No input received. Please try again');
  $input = vio_get_input($prompt, 3, $no_input_msg);
eh_log("input: $input");
    
  switch($input) {
    case -1: // Input error
      eh_error('Error reading input from the user.');
      return -1;
	
    case '': // user didn't type anything. go back
      eh_log('User did not press any extension number.');
      return 0;

    default:
      $new_member_number = $input;
      break;
  }

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $new_member_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
    eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  else if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but %n is not a valid extension number.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but extension %n does not exist.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if (empty($result['extension_info']['uid'])) {
    eh_error("Given extension number is not associated with a user extension: " . serialize($result));
    $prompt = v("I am sorry, but extension %n is not a user extension.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  // check if user is indeed a member of the group
  $user_extension_info = $result['extension_info'];
  $member_name = (empty($user_extension_info['file_audio_name']))
                 ? t('the user from extension %n', array('%n' => $new_member_number))
                 : '*' . $user_extension_info['file_audio_name'];
  if (!in_array($new_member_number, array_keys($subscribers))) {
    $prompt = v("I am sorry, but %name is not a member of %group_name. Only group members are allowed become administrators.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 1;
  }
  // check if user is not an administrator of the group already
  if (in_array($new_member_number, array_keys($admins))) {
    $prompt = v("I am sorry, but %name is already an administrator of %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 1;
  }
  // promote user to group admistrator        
  $r = voip_group_create_admin($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $user_extension_info['uid']);
  if (($r['error_code'] == VOIP_SUCCESS)) {
    $prompt = v("%name is now an administrator of %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 2;
  }
  else {
  	eh_error('Error creating group administrator: ' . (isset($r['error_msg'])?$r['error_msg']:'no error message'));
    $prompt = v("Processing error promoting %name to administrator of %group_name. Please contact your system administrator.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return -1;
  }
}
  
/*
 * Convert an administrator back into a regular member of the specified group
 * 
 * @param $group_extension_info
 * 
 * @param $admins
 *   array of the extension_info for each administrator of the group. indexed by extension number 
 *
 * @return
 *   integer. -1 :: processing failure; set eh_error_msg() with the error description
 *            0 :: no user input received
 *            1 :: success without membership changes.
 *            2 :: success with membership changes.
 */
function _whatsup_group_delete_admin($group_extension_info, $admins) {
   global $wu_voip_server;
   global $wu_user_info;
   
  // determine the group's name
  if (!empty($group_extension_info['file_audio_name'])) {
    $basename = $group_extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $group_extension_info['number']));
  }
  
  // ask for the extension number of the administrator to be demoted
  $prompt = v('Please dial the extension of the administrator that you want to remove. When done, press the pound key.');
  $no_input_msg = v('No input received. Please try again');
  $input = vio_get_input($prompt, 3, $no_input_msg);
eh_log("input: $input");
    
  switch($input) {
    case -1: // Input error
      eh_error('Error reading input from the user.');
      return -1;
	
    case '': // user didn't type anything. go back
      eh_log('User did not press any extension number.');
      return 0;

    default:
      $new_member_number = $input;
      break;
  }

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $new_member_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
    eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  else if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but %n is not a valid extension number.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but extension %n does not exist.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  else if (empty($result['extension_info']['uid'])) {
    eh_error("Given extension number is not associated with a user extension: " . serialize($result));
    $prompt = v("I am sorry, but extension %n is not a user extension.", array('%n' => $new_member_number));
    vio_say($prompt);
    return 1;
  }
  // check if user is indeed an administrator of the group
  $user_extension_info = $result['extension_info'];
  $member_name = (empty($user_extension_info['file_audio_name']))
                 ? t('the user from extension %n', array('%n' => $new_member_number))
                 : '*' . $user_extension_info['file_audio_name'];
  if (!in_array($new_member_number, array_keys($admins))) {
    $prompt = v("I am sorry, but %name is not an administrator of %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 1;
  }
  // convert admistrator back into regular member of the group        
  $r = voip_group_delete_admin($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $user_extension_info['uid']);
  if (($r['error_code'] == VOIP_SUCCESS)) {
    $prompt = v("%name is now back as a regular member of %group_name.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return 2;
  }
  else {
  	eh_error('Error deleting group administrator: ' . (isset($r['error_msg'])?$r['error_msg']:'no error message'));
    $prompt = v("Processing error removing %name from the administration of %group_name. Please contact your system administrator.", array('%name' => $member_name, '%group_name' => $group_name));
    vio_say($prompt);
    return -1;
  }
}
  
/*
 * Manage pending subscriptions for the specified group
 * 
 * @param $group_extension_info
 * 
 * @param $pending
 *   array with the extension_info for each pending subscriber of the group. indexed by extension number 
 *
 * @return
 *   integer. -1 in case of processing failure, 0 to return to the main menu, or 1 in case of success.
 *   In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_group_manage_pending_subscriptions($group_extension_info, $pending) {
   global $wu_voip_server;
   global $wu_user_info;
   
  // determine the group's name
  if (!empty($group_extension_info['file_audio_name'])) {
    $basename = $group_extension_info['file_audio_name'];
    $group_name = "*$basename";
  }
  else {
    $group_name = t('group extension %n', array('%n' => $group_extension_info['number']));
  }

  // report number of pending subscribers
  $count = count($pending);
  $prompt = ($count == 1)?v('There is 1 pending subscriber waiting for your approval.')
                                 :v('There are %n pending subscribers waiting for your approval', array('%n' => $count));
  vio_say($prompt);
  $choices = array();
  foreach ($pending as $subscriber) {
  	$user_name = (!empty($subscriber['file_audio_name']))
                 ? '*' . $subscriber['file_audio_name']
                 : t('the user from extension %n', array('%n' => $subscriber['number']));
    $choices[1] = v('To approve the subscription for %user_name, press 1.', array('%user_name' => $user_name));
    $choices[2] = v('To deny the subscription for  %user_name, press 2.', array('%user_name' => $user_name));
    $choices[3] = v('To skip this user, press 3.');
    $choices['#'] = v('To go back to the member management area, press the pound key');
    $choice = vio_menu($choices);
    switch ($choice) {
      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting pending subscriptions management menu options (choice: $choice)");
        vio_say(v('Processing error. Please contact your system administrator.'));
        return -1;
    
      case 1: // approve subscription
        $r = voip_group_approve($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $subscriber['uid']);
        switch ($r['error_code']) {
          case VOIP_SUCCESS:
            $prompt = v("Subscription approved for %name.", array('%name' => $user_name));
            vio_say($prompt);
            break;
          default:
  	        eh_error('Error approving user subscription: ' . serialize($r));
            $prompt = v("Processing error approving the subscription of %name to %group_name. Please contact your system administrator.", array('%name' => $user_name, '%group_name' => $group_name));
            vio_say($prompt);
            return -1;
        }
        break;

      case 2: // deny subscription
        $r = voip_group_deny($wu_voip_server, $wu_user_info, $group_extension_info['nid_contents'], $subscriber['uid']);
        switch ($r['error_code']) {
          case VOIP_SUCCESS:
            $prompt = v("Subscription denied for %name.", array('%name' => $user_name));
            vio_say($prompt);
            break;
          default:
  	        eh_error('Error denying pending user subscription: ' . serialize($r));
            $prompt = v("Processing error denying the subscription of %name to %group_name. Please contact your system administrator.", array('%name' => $user_name, '%group_name' => $group_name));
            vio_say($prompt);
            return -1;
        }
        break;
        
       case 3: // skip to the next pending user
         break;
    }
  }
  vio_say(v('End of pending subscriptions list.'));
  return 1;
}


/*
 * Record a voicemail for the specified extension.
 *
 * @param $voicemail_server
 * URL of the xmlrpc.php of the site where the message is to be stored
 * 
 * @param $destination_list
 *   array with the extension numbers the message is to be sent to
 *
 * @param @destin_extension
 *   int with the number of the destin extension to be associated with the message (mainly used for group voicemail)
 * 
 * @return
 *   integer. -1 in case of processing failure, 0 in case of non-fatal failure, or 1 in case of success.
 *   In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_voicemail_message_record($destination_list, $destin_extension) {
  global $wu_user_info;
  global $wu_voip_server;
  
  // record the voicemail message
// TODO: replace the prompt bellow by the greeting associated with the mailbox
  $prompt = v("Please record your message after the beep.  When done, press the pound key.");
  $file =  vio_record_audio($prompt);
eh_log("This is the file just recorded: $file");
  if ($file == -1) {
    vio_say(v("Error recording file.  Please contact your system administrator."));
    eh_error('Error recording voicemail message file.');
    return -1;
  }
    
  $msg_info = array();
  $msg_info['destin_extension'] = $destin_extension;
  $msg_info['caller_id'] = $wu_user_info['caller_id'];
  $msg_info['audio_file'] = $file;
  
  $rc = voip_voicemail_msg_create($wu_voip_server, $wu_user_info, $destination_list, $msg_info);
  
  switch ($rc['error_code']) {
  	case VOIP_SUCCESS:
  	  $prompt = v("Your message will be delivered in a couple of minutes. Thank you.");
      vio_say($prompt);
      return 1;
      
    case VOIP_LACK_OF_PERMISSION:  
      $prompt = v("You do not have the necessary permissions to send voicemail messages.");
      vio_say($prompt);
      return 0;
      
    default:  
    case VOIP_PROCESSING_FAILURE:
    case VOIP_INVALID_USER_INFO:
      $prompt = v('Processing error. Please contact your system administrator.');
      vio_say($prompt);
      eh_error('Error creating voicemail message: ' . $rc['error_msg']);
      return -1;
  }
}

/*
 * Create a new user group.
 *
 * @return
 *   integer. -1 in case of processing failure or 1 in case of success.
 *   In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_group_create_group() {
  global $wu_voip_server;
  global $wu_user_info;
  
  $group_info = array();
  
  // record the group name
  $prompt = v("Please record the group name after the beep.  When done, press the pound key.");
  $file_audio_name =  vio_record_audio($prompt);
  if ($file_audio_name == -1) {
    vio_say("Error recording group name.  Please try again later.");
    eh_error('Error recording group name.');
    return -1;
  }

  // record the group description
  $prompt = v("Please wait for the beep and then record a brief description about your group.  When done, press the pound key.");
  $file_audio_description =  vio_record_audio($prompt);
  if ($file_audio_description == -1) {
    vio_say("Error recording group description.  Please contact your system administrator.");
    eh_error('Error recording group description.');
    return -1;
  }

  // now determine the kind of member subscription that should be offered 
  vio_say('Now determine how you would like this group to handle subscription requests.');
  
  $choices = array();
  $choices[1] = v('To allow open subscriptions from anyone, press 1.');
  $choices[2] = v('To have subscriptions be moderated by group administrators, press 2.');
  $choices['#'] = v('To cancel this operation, press the pound key.');
  $choice = vio_menu($choices);
  switch($choice) {
    default: // invalid option
    case -1: // processing error
      eh_error("Error selecting group creation options (choice: $choice)");
      $prompt = v('Processing error. Please contact your system administrator.');
      vio_say($prompt);
      return -1;
    
    case 1: // open subscription
      $group_info['og_selective'] = 0;
      break;

    case '2': // moderated subscription
      $group_info['og_selective'] = 1;
      break;
      
    case '#': // cancel group creation
      vio_say('Cancelling group creation.');
      $r = unlink($file_audio_name);
      return 1;
  }
  
  $rc = voip_group_create_group($wu_voip_server, $wu_user_info, $group_info, $file_audio_name, $file_audio_description);
  switch ($rc['error_code']) {
  	case VOIP_SUCCESS:
  	  $prompt = v("Your new group has been created with extension number %number.", array('%number' => $rc['extension_info']['number']));
      vio_say($prompt);
      return 1;
      
    case VOIP_LACK_OF_PERMISSION:  
      $prompt = v("I am sorry, but you do not have the necessary permissions to create groups.");
      vio_say($prompt);
      return 1;
      
    default:  
    case VOIP_PROCESSING_FAILURE:
    case VOIP_INVALID_USER_INFO:
      $prompt = v('Processing error. Please contact your system administrator.');
      vio_say($prompt);
      eh_error('Error creating voip group: ' . $rc['error_msg']);
      return -1;
  }
}


/*
 * Record the audio contents for either the name or description fields of the specified extension.
 *
 * @param $extension_number
 * int.
 * 
 * @param $field_name
 * string with either 'audio_name' or 'audio_description'
 *
 * @return
 *   integer. -1 in case of processing failure,  or 1 in case of success.
 */
function _whatsup_extension_record_audio_field( $extension_number, $field_name) {
  global $wu_voip_server;
  global $wu_user_info;
  
  // record the field contents
  if ($field_name == 'audio_name') {
    $prompt = v("Please record the extension name after the beep.  When done, press the pound key.");
  }
  elseif ($field_name == 'audio_greetings') {
    $prompt = v("Please record the welcome message after the beep.  When done, press the pound key.");
  }
  else {
    $prompt = v("Please record the extension description after the beep.  When done, press the pound key.");
  }
  $file_name =  vio_record_audio($prompt);
eh_log("This is the file just recorded: $file_name");
  if ($file_name == -1) {
    vio_say("Error recording file.  Please try again later.");
    return -1;
  }
    
  $rc = voip_extension_set_audio_field($wu_voip_server, $wu_user_info, $extension_number, $field_name, $file_name);
  
  switch ($rc['error_code']) {
  	case VOIP_SUCCESS:
  	  if ($field_name == 'audio_name') {
  	    $prompt = v("The extension name will be updated in a couple of minutes. Thank you.");
  	  }
      elseif ($field_name == 'audio_greetings') {
        $prompt = v("The welcome message will be updated in a couple of minutes. Thank you.");
      }
  	  else {
  	    $prompt = v("The extension description will be updated in a couple of minutes. Thank you.");
  	  }
      vio_say($prompt);
      return 1;
      
    case VOIP_LACK_OF_PERMISSION:  
      $prompt = v("I am sorry, but you do not have the necessary permissions to update information associated with this extension.");
      vio_say($prompt);
      return 0;
      
    default:  
    case VOIP_PROCESSING_FAILURE:
    case VOIP_INVALID_USER_INFO:
      $prompt = v('Processing error. Please contact your system administrator.');
      vio_say($prompt);
      return -1;
  }
}

/*
 * Record a new audioblog entry to be uploaded.
 * 
 * @param $extension_info
 * array with the information associated with the extension the entry is to be published for. This is either a user or a group extension.
 * 
 * @param $is_group_admin
 * boolean. When recording an entry for a group, only group administrators are provided with the option to publish the entry in the front page of the website.
 *
 * @return
 *   integer. -1 indicates processing failure, 0 indicates that the operation 
 *     has been cancelled, 1 indicates success. If -1, check eh_error_msg() for explanation.
 */
function _whatsup_audioblog_create($extension_info, $allow_front_page = FALSE) {
eh_log("_whatsup_audioblog_create");

  global $wu_voip_server;
  global $wu_user_info;
  
  // initialize local variables
  $rc = 1;
  $is_group_audioblog = (isset($extension_info['nid_contents']) && ($extension_info['type_contents'] == 'voip_group'))?TRUE:FALSE;
  
  // Make sure user is logged in...

  $r = _whatsup_login();
  if ($r != 1) {
    return $r;
  }
  
  // now record the entry
  $recording_audioblog = TRUE;
  while ($recording_audioblog) {

    $prompt = v("Please record your audioblog entry after the beep.  When done, press the pound key.");
    $file =  vio_record_audio($prompt);
eh_log("This is the file just recorded: $file");
    if ($file == -1) {
      return -1;
    }

    $confirming_options = TRUE;

    $audioblog_info = array();
    $audioblog_info['file'] = $file;
    $audioblog_info['status'] = 1; // published
    $audioblog_info['promote'] = FALSE;
    $audioblog_info['sticky'] = FALSE;
    $audioblog_info['downloadable'] = TRUE;
    $audioblog_info['categories'] = array();
    $audioblog_info['og_public'] = $is_group_audioblog? FALSE : TRUE;
    $audioblog_info['og_groups'] = $is_group_audioblog? array($extension_info['nid_contents'] => TRUE) : array();
    $audioblog_info['date_created'] = time();

    $choices = array();
    $choices[1] = v("To upload the new audioblog entry, press 1.");
    $choices[2] = v("To select the categories to be associated with this entry, press 2.");

    // determine the sender's name
    if (!empty($extension_info['file_audio_name'])) {
      $extension_name = '*'. $extension_info['file_audio_name'];
    }
    else {
      $extension_name = t('extension %n', array('%n' => $extension_info['number']));
    }
 
    if ($is_group_audioblog) {
      $prompt_public = v('To make this entry also available to non-members of %group_name, press 3.', array('%group_name' => $extension_name));
      $prompt_group_only = v('To make this entry only available to members of %group_name, press 3.', array('%group_name' => $extension_name));
      $choices[3] = '';
    }
    if ((!$is_group_audioblog) || ($is_group_audioblog && $allow_front_page)) {
      $prompt_promote = v("To publish this entry on the front page of the website, press 4.");
      $prompt_dont_promote = v("To not publish this entry on the front page of the website, press 4.");
      $choices[4] = '';
    }
    $choices[5] = v('To replay your audioblog entry, press 5.');
    $choices[6] = v('To record your entry once again, press 6.');
    $choices['#'] = v("To cancel this operation, press the pound key.");

    while ($confirming_options) {

      if ($is_group_audioblog) {
        $choices[3] = ($audioblog_info['og_public'])?$prompt_group_only:$prompt_public;
      }
      if ((!$is_group_audioblog) || ($is_group_audioblog && $allow_front_page)) {
        $choices[4] = ($audioblog_info['promote']) ? $prompt_dont_promote : $prompt_promote;
      }
      
      $choice = vio_menu($choices);

      switch ($choice) {

        case 1: // Publish the new entry
          // render body and title
          $template_args['extension_number'] = $extension_info['number'];
          $template_args['person_name'] = $extension_info['name'];
          $template_args['caller_id'] = $wu_user_info['caller_id'];
          $template_args['date_created'] = $audioblog_info['date_created'];
          $template_args['public'] = $audioblog_info['og_public'];
          $title_template = 'audioblog_title.php';
          $template_file = ('templates' . DIRECTORY_SEPARATOR . $title_template);
          $title = render_output($template_file, $template_args);
//$title = ($audioblog_info['og_public'] == TRUE)?t('Public audioblog entry'):t('Group-only audioblog entry');
          $audioblog_info['title'] = $title;
          $body_template = 'audioblog_body.php';
          $template_file = ('templates' . DIRECTORY_SEPARATOR . $body_template);
          $audioblog_info['body'] = render_output($template_file, $template_args);
          $artist = $extension_info['name'];
          $tags = array('title' => $title, 'artist' => $artist, 'genre' => 'Other');
          $audioblog_info['tags'] = $tags;
          // add new entry to the upload queue
          $r = asa_add_audio_entry_to_upload_queue($wu_voip_server, $wu_user_info, $audioblog_info);
          if (!$r) {
            $rc = -1;
          }
          else {
            $prompt = v("Your new audioblog entry will be uploaded in a couple of minutes. Thank you.");
            vio_say($prompt);
          }
          $confirming_options = FALSE;
          $recording_audioblog = FALSE;
          break;

        case 2: // select entry categories
          $category_list = asa_get_audio_category_list($wu_voip_server);
          $accept_none = TRUE;
          $accept_multiple = TRUE;
          $accept_any = FALSE;
          $r = _whatsup_get_categories($category_list, $wu_voip_server, $accept_none, $accept_multiple, $accept_any);
          if (!$r->success) {
            $rc = -1;
            $confirming_options = FALSE;
            $recording_audioblog = FALSE;
          }
          else {
            $audioblog_info['categories'] = $r->selected_ids;
eh_log("Categories selected: " . serialize($audioblog_info['categories']));
          }
          break;

        case 3: // set public / group-only accessibility flag
          $audioblog_info['og_public'] = ($audioblog_info['og_public']) ? FALSE : TRUE;
          $prompt = ($audioblog_info['og_public'])
                    ? v('This audioblog entry will be available to non-members of %group_name.', array('%group_name' => $extension_name))
                    : v('This audioblog entry will only be available to members of %group_name.', array('%group_name' => $extension_name));
          vio_say($prompt);
          break;

        case 4: // set front page / non-front page flag
          $audioblog_info['promote'] = ($audioblog_info['promote']) ? FALSE : TRUE;
          $prompt = ($audioblog_info['promote']) ? v('Entry will be published in the front page.') : v('Entry will not be published in the front page.');
          vio_say($prompt);
          break;
          
        case 5: // replay
          $basename = basename($file, '.wav');
          $dir = dirname($file);
          vio_say(v('Playing recorded entry: %file.', array('%file' => "*$dir/$basename")));
          break;
          
        case 6: // re-record
          $confirming_options = FALSE;
          break;

        case '#': // go back to the main menu
          $prompt = v('Cancelling operation and going back to the main menu.');
          vio_say($prompt);
          $rc = 0;
          $confirming_options = FALSE;
          $recording_audioblog = FALSE;
          break;

        default: // invalid choice
          eh_error('Invalid choice selected when recording new audioblog entries.');
          $rc = -1;
          $confirming_options = FALSE;
          $recording_audioblog = FALSE;
          break;
      }
    }
  }

  if ($rc != 1) {
    unlink($file);
  }

  return $rc;
}

/*
 * @return
 *   int. -1 :: processing error
 *         1 :: success, go to the next batch of events
 *         2 :: user chose to go back to calling extension
 */
function _whatsup_event_present_events($event_list, $calling_extension_name, $time_only = FALSE, $additional_events = FALSE) {
eh_log("_whatsup_event_present_events($event_list, $calling_extension_name)");

  global $wu_user_info;
  global $wu_voip_server;

  // initialize user menus 
  
  $choices1 = array();
  $choices1[1] = v('To know more about this event, press 1.');
  if ($additional_events || (count($event_list) > 1)) {
    $choices1[9] = v('To move to the next event, press 9');
  }
  $choices1['#'] = v('To go back to %extension_name, press the pound key', array('%extension_name' => $calling_extension_name));

  foreach ($event_list as $event_info) {
    $organizer_extension_info =& $event_info['organizer_extension_info'];

/*****
    //determine the name of the organizer
    if (isset($organizer_extension_info['file_audio_name'])) {
      $basename = $organizer_extension_info['file_audio_name'];
      $organizer_name = "*$basename";
    }
    else if (isset($organizer_extension_info['name'])) {
      $organizer_name = $organizer_extension_info['name'];
    }
    else {
      $organizer_name = t('extension %n', array('%n' => $organizer_extension_info['number']));
    }
******/

    // determine the event title
    if (isset($event_info['extension_info']['file_audio_name'])) {
      $basename = $event_info['extension_info']['file_audio_name'];
      $event_title = "*$basename";
    }
    else if (isset($event_info['extension_info']['name'])) {
      $event_title = $event_info['extension_info']['name'];
    }
    else {
      $event_title = t('Event organized by extension %extension_number', array('%extension_number' => $organizer_extension_info['number']));
    }
 
    // determine the event date
    $ts_start = $event_info['ts_start'];
/** 
    if (!$time_only) {
      $event_date = t("on %date: ", array('%date' => _whatsup_friendly_date_and_time($ts_start)));
    }
    else {
      if ($ts_start % 3600)
        $event_date = t("at %time: ", array('%time' => date('g:i A', $ts_start)));
      else {
        $event_date = t("at %time: ", array('%time' => date('g A', $ts_start)));
      }
    }
***/
    // play the event date and title
/*****
    if ($ts_start > time()) {
      vio_say(v('%on_event_date, %organizer_name will be organizing %event_title.', array('%on_event_date' => $event_date, '%organizer_name' => $organizer_name, '%event_title' => $event_title)));
    }
    else if ($event_info['ts_end'] > time()) {
      vio_say(v('%on_event_date, %organizer_name started %event_title, which is still happening.', array('%on_event_date' => $event_date, '%organizer_name' => $organizer_name, '%event_title' => $event_title)));
    }
    else {
      vio_say(v('%on_event_date, %organizer_name organized %event_title.', array('%on_event_date' => $event_date, '%organizer_name' => $organizer_name, '%event_title' => $event_title)));
    }
****/
      $event_date = _whatsup_friendly_date_and_time($ts_start);
      vio_say(v('%event_title %event_date', array('%event_date' => $event_date, '%event_title' => $event_title)));

    // update access statistics associated with the event just read
    $rc2 = @asa_update_audio_entry_statistics($wu_voip_server, $wu_user_info, $event_info['extension_info']['nid_contents']);
    if (!$rc2) {
eh_log("Error updating event statistics: " . eh_error_msg());
      vio_say(v("Processing error.  Please contact your system administrator."));
      return WU_PROCESSING_FAILURE;
    }
        
    // present options to user
    $choice = vio_menu($choices1);
    switch ($choice) {
      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting event menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        return -1;
      case 1: // find out more about event
        $rc = _whatsup_event_play($event_info, $calling_extension_name, FALSE); 
        switch ($rc) {
          case -1:
            eh_error("Invalid event play option returned: $rc.");
            return WU_PROCESSING_FAILURE;
          case 4: // skip to next event
          case 1: // success, move to next event
            break;
          case 2: // return to calling extension
            return WU_GO_BACK;
          case 3: // go back to main menu
            return WU_GO_TO_MAIN_MENU;
          default:
            eh_error("Processing error reading upcoming events: $rc.");
            $prompt = v("Processing error. Please contact your system administrator.");
    	    vio_say($prompt);
    	    return WU_PROCESSING_FAILURE;
        }
        break;
      case 9: // go to the next event
        break;
      case '#': // go back to calling extension
        return WU_GO_BACK;
    }
  }
  return WU_SUCCESS;
}        

/*
 * @return
 *   integer -1 in case of processing error, 0 in case of recoverable error,
 *   1 in case of success. In case of failure, set eh_error_msg() with the error description.
 */
function _whatsup_event_review() {
  global $wu_voip_server;
  global $wu_user_info;
  
  // ask for the event extension
  
  $prompt = v('Please dial the event extension. When done, press the pound key.');
  $no_input_msg = v('No input received. Please try again.');
  $input = vio_get_input($prompt, 3, $no_input_msg);
 eh_log("input: $input");
    
  switch($input) {
	case -1: // Input error
	  eh_error('Error reading input from the user.');
	  return -1;
	
    case '': // user didn't type anything. go back
      eh_log('User did not press any extension.');
      return 0;
  
    default:
      $extension_number = $input;
      break;
  }

  // check if the desired extension is valid.
  $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $extension_number);
eh_log('result: '. serialize($result));  
  if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
  	eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
    return -1;	
  }
  if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("%extension_number is not a valid extension number.", array('%extension_number' => $extension_number));
    vio_say($prompt);
    return 0;
  }

  if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
    eh_error("Invalid extension: {$result['error_msg']}");
    $prompt = v("I am sorry, but extension %extension_number does not exist.", array('%extension_number' => $extension_number));
    vio_say($prompt);
    return 0;
  }
  
  $extension_info = $result['extension_info'];
  if ($extension_info['type_contents'] != 'voip_event') {
    vio_say(v("%extension_number is not an event extension number.", array('%extension_number' => $extension_number)));
    return 0;
  }

  // retrieve event-specific information from the server
  $rc = voip_event_get_event_from_nid($wu_voip_server, $wu_user_info, $extension_info['nid_contents']);
  
  if ($rc['error_code'] != VOIP_SUCCESS) {
  	return -1;
  }
  
  // present event info to the caller
  $event_info = $rc['event_info'];
  $calling_extension_name = 'event management area';
  $as_extension = TRUE;
  $r = _whatsup_event_play($event_info, $calling_extension_name, $as_extension);
  if ($r == -1) {
    return -1;
  }
  else {
    return 1;
  }

}

/*
 * 
 */
function _whatsup_event_area() {
  global $wu_voip_server;
  global $wu_user_info;
eh_log("whatsup_event_area()");

  $rc = 1;
  
  // remind user to login
  $prompt_login_option = v('Only registered users have access to non-public events. To loggin, press 1.');
  $prompt_other_option = v('To remain anonymous, press the pound key.');
  $r = _whatsup_check_login($prompt_login_option, $prompt_other_option);
  if ($r == -1) {
    return WU_PROCESSING_FAILURE;
  }

  $choices = array();
  $choices[1] = v('Community calendar.  To listen to upcoming community events, press 1.');
  $choices[2] = v('To listen to upcoming events from specific categories, press 2.');
  $choices[3] = v('To listen to events that start after a specific date, press 3.');
  $choices['#'] = v('To go back to the main menu, press the pound key.');

  $events_menu = TRUE;
  while ($events_menu) {
    vio_beep();
    $choice = vio_menu($choices);
    switch($choice) {
      case -1:
        eh_error("Error selecting events menu options");
        $rc = -1;
        $events_menu = FALSE;
        break;
  
      case '#': // back to main menu
        $rc = 0;
        $events_menu = FALSE;
        break;

      case 1: // check upcoming community events
      case 2: // check upcoming events from the specified categories
        $timestamp = time();
        $start = array();
        $start['year'] = date('Y', $timestamp);
        $start['month'] = date('n', $timestamp);
        $start['day'] = date('j', $timestamp);
        $start['hour'] = date('G', $timestamp);
        $start['minute'] = date('i', $timestamp);
        $start['second'] = date('s');
        $query_info['start'] = $start;
        $query_info['end'] = NULL;
        $query_info['limit'] = NULL;
        if ($choice == 1) {
          $query_info['terms'] = NULL;
          $prompt_no_events = v('I am sorry, but there are no events currently planned. Please try again later.');
        }
        else {
          // ask user to select desired categories
          $category_list = voip_get_category_list($wu_voip_server);
          $accept_none = FALSE;
          $accept_multiple = TRUE;
          $accept_any = TRUE;
          $any_categories_txt = "To select events from all categories, press %key.";
          $r = _whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $any_categories_txt);
          if (!$r->success) {
            $prompt = v("Error selecting categories from the server. Please contact your system administrator.");
            vio_say($prompt);
            return -1;
          }
          $query_info['terms'] = $r->selected_ids;
          $prompt_no_events = v('I am sorry, but there are no upcoming events under the specified categories. Please try again later.');
        }
        $query_info['organizer_extension_number'] = NULL;
        $query_info['gid'] = NULL;
        $prompt_end_of_events = v('End of upcoming events.');
        $prompt_description = v('Upcoming events');
        $txt_calling_extension_name = t('Community calendar.');
        $rc = _whatsup_event_query_and_play($wu_voip_server, $wu_user_info, $query_info, $prompt_no_events, $prompt_end_of_events, $prompt_description, $txt_calling_extension_name);
        switch ($rc) {
          default:
            return WU_PROCESSING_FAILURE;
          case WU_PROCESSING_FAILURE: 
          case WU_GO_TO_MAIN_MENU:
            return $rc;
          case WU_GO_BACK: 
          case WU_SUCCESS: 
            break;
        }
        break;

      case 3: // events after a certain date
        $prompt_date = v('Please enter the start date. Use 2 digits for the month, 2 digits for the day, and 4 digits for the year.');
        $prompt_invalid_date = v('Invalid date. Please try again.');
        $prompt_cancel_txt = 'To go back to the community calendar extension, press %key';
        $date = _whatsup_get_date($prompt_date, $prompt_invalid_date, $prompt_cancel_txt);
        if ($date == -1) {
          return -1;
        }
        if ($date == 0) {
          break;
        }
        $start['year'] = $date['year'];
        $start['month'] = $date['month'];
        $start['day'] = $date['day'];
        $start['hour'] = 0;
        $start['minute'] = 0;
        $start['second'] = 0;
        $query_info['start'] = $start;
        $query_info['end'] = NULL;
        $query_info['limit'] = NULL;
        $query_info['terms'] = NULL;
        $query_info['organizer_extension_number'] = NULL;
        $query_info['gid'] = NULL;
        $query_info['organizer_extension_number'] = NULL;
        $query_info['gid'] = NULL;
        $prompt_no_events = v('I am sorry, but there are no events currently planned for after the specified date.');
        $prompt_end_of_events = v('End of event listing.');
        $ts_date = mktime($start['hour'], $start['minute'], $start['second'], $start['month'], $start['day'], $start['year']);
        $prompt_description = v('Events starting after %date.', array('%date' => _whatsup_friendly_date($ts_date)));
        $txt_calling_extension_name = t('Community calendar.');
        $rc = _whatsup_event_query_and_play($wu_voip_server, $wu_user_info, $query_info, $prompt_no_events, $prompt_end_of_events, $prompt_description, $txt_calling_extension_name);
        switch ($rc) {
          default:
            return WU_PROCESSING_FAILURE;
          case WU_PROCESSING_FAILURE: 
          case WU_GO_TO_MAIN_MENU:
            return $rc;
          case WU_GO_BACK: 
          case WU_SUCCESS: 
            break;
        }
        break;
    }
  }
  return $rc;
}

function _whatsup_event_query_and_play($wu_voip_server, $wu_user_info, $query_info, $prompt_no_events, $prompt_end_of_events, $prompt_description = NULL, $txt_calling_extension_name = NULL) {
  $start = $query_info['start'];
  $count_events = 0;
  $limit = 1; // count until query_info['limit'];
  $query_events = TRUE;
eh_log('query_info: ' . print_r($query_info, TRUE));
  while ($query_events) {
    $result = voip_event_get_events($wu_voip_server, $wu_user_info, $start, $query_info['end'], $limit, $query_info['terms'], $query_info['organizer_extension_number'], $query_info['gid']);
eh_log('get events: ' . print_r($result, TRUE));
    if ($result['error_code'] != VOIP_SUCCESS) {
      vio_say(v("Processing error. Please contact your system administrator."));
      $query_events = FALSE;
      $rc = WU_PROCESSING_FAILURE;
      continue;
    }
    $event_list =& $result['event_list'];
    if (empty($event_list)) {
      if ($count_events == 0) {
        vio_say($prompt_no_events);
      }
      else {
        vio_say($prompt_end_of_events);
      }      	
      $query_events = FALSE;
      $rc = WU_SUCCESS;
      continue;
    }  
    if (($count_events == 0) && $prompt_description) {     
      vio_say($prompt_description);  
    }
    // now present the event just retrieved
    $count_events++;
    $time_only = FALSE; // read both event date and time
    $additional_events = TRUE;
    $r = _whatsup_event_present_events($event_list, $txt_calling_extension_name, $time_only, $additional_events);
    switch ($r) {
      default:
        eh_log(t('Processing error: invalid option returned by _whatsup_event_present_events: %option', array('%option' => $r)));
        vio_say(v("Processing error. Please contact your system administrator."));
        $rc = WU_PROCESSING_FAILURE;
        $query_events = FALSE;
        break;
        
      case WU_PROCESSING_FAILURE: 
      case WU_GO_BACK:
      case WU_GO_TO_MAIN_MENU:
        $rc = $r;
        $query_events = FALSE;
        break;
        
      case WU_SUCCESS: 
        $rc = $r;
        if ($query_info['limit'] && ($count_events >= $query_info['limit'])) {
          $query_events = FALSE;
        }
        else {
          // query the next event
          $event_descriptor = current($event_list);
          $start = $event_descriptor['start'];
          $start['second']++;
        } 
        break;
    }
  } // while query_events
  
  return $rc;
}

/*
 * Create a new calendar event.
 * 
 * @param $organizer_extension_info
 * extension of the user or group organizing the event
 * 
 * @param $allow_public_events
 * boolean. TRUE if the event is to be visible by non-members of the group This option is only considered if the organizer of the event is a group. 
 *
 * @return
 *   integer. -1 indicates processing failure, 0 indicates that the operation 
 *     has been cancelled, 1 indicates success. If -1, check eh_error_msg() for explanation.
 */
function _whatsup_event_create_event($organizer_extension_info, $allow_public_events = FALSE) {
eh_log("_whatsup_event_create_event($organizer_extension_info, $allow_public_events)");
eh_log('organizer_extension_info: ' . serialize($organizer_extension_info));

  global $wu_voip_server;
  global $wu_user_info;
  
  $category_ids = array();
  
  // determine the organizer's name
  if (isset($organizer_extension_info['file_audio_name'])) {
    $basename = $organizer_extension_info['file_audio_name'];
    $organizer_name = "*$basename";
  }
  else {
  	if (!empty($extension_info['uid'])) {
      $organizer_name = t('user from extension %n', array('%n' => $organizer_extension_info['number']));
  	}
  	else {
      $organizer_name = t('group from extension %n', array('%n' => $organizer_extension_info['number']));
  	}
  }
  
  $choices = array();
//  $choices[1] = t('In order to create a community event, you will need to know the event title, the event description, and the event dates, and also the event location. If you want to proceed, press 1.');
  $choices[1] = t('In order to create a community event, you will need to know the event title, the event description, and also the event date and time. If you want to proceed, press 1.');
  $choices['#'] = t('If you want to go back to the group extension menu, press the pound key');
  $choice = vio_menu($choices);
  switch($choice) {
  	case 1: // create the event
  	  break;
    case '#':
      return 0;
    default:
      eh_error_msg('Error selecting create community event choices: ' . serialize($choice));
      vio_say(t('Processing error. Please contact your system administrator.'));
      return -1;
  }
  
  // now record the event information
    $event_info = array();
    
    // record the event name
    $prompt = v("Please record the event title after the beep.  When done, press the pound key.");
    $file_audio_name =  vio_record_audio($prompt);
    if ($file_audio_name == -1) {
      vio_say("Error recording event title.  Please contact your system administrator.");
      eh_error('Error recording event title.');
      return -1;
    }

    // record the event description
    $prompt = v("Please wait for the beep and then record a brief description about the event.  When done, press the pound key.");
    $file_audio_description =  vio_record_audio($prompt);
    if ($file_audio_description == -1) {
      unlink($file_audio_name);
      vio_say("Error recording event description.  Please contact your system administrator.");
      eh_error('Error recording event description.');
      return -1;
    }

    // record dates and times
    $start_only = TRUE;
    $dates_and_times = _whatsup_event_get_dates_and_times($start_only);
    if (!is_array($dates_and_times)) {
      unlink($file_audio_name);
      unlink($file_audio_description);
      return $dates_and_times;
    }

/************
    // record the event location
    $prompt = v("Please record the event location.  When done, press the pound key.");
    $file_audio_location =  vio_record_audio($prompt);
    if ($file_audio_location == -1) {
      unlink($file_audio_name);
      unlink($file_audio_description);
      vio_say("Processing error.  Please contact your system administrator.");
      eh_error('Error recording event description.');
      return -1;
    }
************/
    
    // by default, individually-organized events are public and group-organized events are group-only
    $is_public = empty($organizer_extension_info['uid'])?FALSE:TRUE;
    
    // determine which groups the event should be associated with
    $event_groups = (!empty($organizer_extension_info['uid']))? NULL : array($organizer_extension_info['nid_contents'] => TRUE);
    
    // organize the menu
    $choices_accept_event = array();
    $choices_accept_event[1] = t('To publish this event, press 1.');
    $choices_accept_event[2] = t('To replay what you have thus far, press 2.');
    if (empty($organizer_extension_info['uid']) && ($allow_public_events)) {
      $choices_accept_event[3] = ''; // reserve slot
    }
    $choices_accept_event[4] = v('To select the categories associated with this event, press 4.');
    $choices_accept_event[5] = t('To change the title of the event, press 5.');
    $choices_accept_event[6] = t('To change the description of the event, press 6.');
    $choices_accept_event[7] = t('To change the event dates and times, press 7.');
//    $choices_accept_event[8] = t('To change the event location, press 8.');
    $choices_accept_event['#'] = t('To cancel the creation of this event, press the pound key');
    

    $choosing = TRUE;
    while ($choosing) {
      // ask user to confirm event options 
      if (empty($organizer_extension_info['uid']) && ($allow_public_events)) {
        if ($is_public) {
          $choices_accept_event[3] = v('To make this event only accessible to members of %organizer_name, press 3.', array('%organizer_name' => $organizer_name));
        }
        else {
          $choices_accept_event[3] = v('To make this event also accessible to non-members of %organizer_name, press 3.', array('%organizer_name' => $organizer_name));
        }
      }
      $choice = vio_menu($choices_accept_event);
      switch ($choice) {
        case 1: // publish the event
          $event_info['title'] = t('New event organized by %organizer_name', array('%organizer_name' => $organizer_extension_info['name'] ));
          $event_info['body'] = t('New event organized by %organizer_name', array('%organizer_name' => $organizer_extension_info['name'] ));
          $event_info['status'] = 1; // published
          $event_info['promote'] = TRUE; // TODO: should all events go to the front page?
          $event_info['categories'] = $category_ids;
          $event_info['og_public'] = $is_public;
          $event_info['og_groups'] = $event_groups;
          $event_info['start'] = $dates_and_times['array_start'];
          $event_info['end'] = $dates_and_times['array_end'];
          $event_info['organizer_extension_number'] = $organizer_extension_info['number'];
//          $rc = voip_event_create_event($wu_voip_server, $wu_user_info, $event_info, $file_audio_name, $file_audio_description, $file_audio_location);    
          $rc = voip_event_create_event($wu_voip_server, $wu_user_info, $event_info, $file_audio_name, $file_audio_description);    
          switch ($rc['error_code']) {
  	        case VOIP_SUCCESS:
  	          $prompt = v("Your new event has been created with extension number %number.", array('%number' => $rc['extension_info']['number']));
              vio_say($prompt);
              return 1;
      
            case VOIP_LACK_OF_PERMISSION:  
              $prompt = v("I am sorry, but you do not have the necessary permissions to create events.");
              vio_say($prompt);
              return 1;
      
            default:  
            case VOIP_PROCESSING_FAILURE:
            case VOIP_INVALID_USER_INFO:
              $prompt = v('Processing error. Please contact your system administrator.');
              vio_say($prompt);
              eh_error('Error creating voip event: ' . $rc['error_msg']);
              return -1;
          }
          break;        

        case 2: // play back
          $basename = basename($file_audio_name, '.wav');
          $dir = dirname($file_audio_name);
          vio_say(v('Event title: %file.', array('%file' => "*$dir/$basename")));
          $basename = basename($file_audio_description, '.wav');
          $dir = dirname($file_audio_description);
          vio_say(v('Event description: %file.', array('%file' => "*$dir/$basename")));
          vio_say(t('Event start date: %event_start.', 
                    array('%event_start' => date('l, M j, Y @ g:i A', $dates_and_times['timestamp_start']))));
/***********
          vio_say(t('Event end date: %event_end.', 
                    array('%event_end' => date('l, M j, Y @ g:i A', $dates_and_times['timestamp_end']))));
          $basename = basename($file_audio_location, '.wav');
          $dir = dirname($file_audio_location);
          vio_say(v('Event location: %file.', array('%file' => "*$dir/$basename")));
************/
          if ($is_public) {
            $prompt = v('This is a public event being organized by: %organizer_name', array('%organizer_name' => $organizer_name));
          }
          else {
            $prompt = v('This is a group-only event being organized by: %organizer_name', array('%organizer_name' => $organizer_name));
          }
          vio_say($prompt);
          break;

        case 3: // switch between public or group-only events
          $is_public = ($is_public) ? FALSE : TRUE;
          $prompt = ($is_public)
                    ? v('This event will be available to non-members of %organizer_name.', array('%organizer_name' => $organizer_name))
                    : v('This event will only be available to members of %organizer_name.', array('%organizer_name' => $organizer_name));
          vio_say($prompt);
          break;

        case 4: // select event categories
          $category_list = voip_get_category_list($wu_voip_server);
          $accept_none = TRUE;
          $accept_multiple = TRUE;
          $accept_any = FALSE;
          $r = _whatsup_get_categories($category_list, $wu_voip_server, $accept_none, $accept_multiple, $accept_any);
          if (!$r->success) {
            return -1;
          }
          else {
            $category_ids = $r->selected_ids;
          }
          break;

        case 5: // change title
          unlink($file_audio_name);
          $prompt = v("Please record the event title after the beep.  When done, press the pound key.");
          $file_audio_name =  vio_record_audio($prompt);
          if ($file_audio_name == -1) {
             unlink($file_audio_description);
             vio_say("Error recording event title.  Please contact your system administrator.");
            eh_error('Error recording event title.');
            return -1;
          }
          break;

        case 6: // change description
          $prompt = v("Please wait for the beep and then record a brief description about the event.  When done, press the pound key.");
          $file_audio_description =  vio_record_audio($prompt);
          if ($file_audio_description == -1) {
            unlink($file_audio_name);
            unlink($file_audio_description);
//            unlink($file_audio_location);
            vio_say("Error recording event description.  Please contact your system administrator.");
            eh_error('Error recording event description.');
            return -1;
          }
          break;

        case 7: // change dates and times
          $dates_and_times = _whatsup_event_get_dates_and_times(TRUE);
          if (!is_array($dates_and_times)) {
            unlink($file_audio_name);
            unlink($file_audio_description);
//            unlink($file_audio_location);
            return $dates_and_times;
          }
          break;

        case 8: // change location
          $prompt = v("Please record the event location.  When done, press the pound key.");
          $file_audio_location =  vio_record_audio($prompt);
          if ($file_audio_location == -1) {
            unlink($file_audio_name);
            unlink($file_audio_description);
            vio_say("Processing error.  Please contact your system administrator.");
            eh_error('Error recording event location.');
            return -1;
          }
          break;

        case '#': // cancel
          unlink($file_audio_name);
          unlink($file_audio_description);
          unlink($file_audio_location);
          return 0;

        default:
          unlink($file_audio_name);
          unlink($file_audio_description);
          unlink($file_audio_location);
          eh_error_msg("Invalid option selected in event creation: $choice");
          vio_say(t('Processing error. Please contact your system administrator.'));
          return -1;
      }
    } // while choosing
  
  return 1;
}

/*
 * Process extension of a specific event
 */
function _whatsup_event_extension($extension_info) {
  global $wu_voip_server;
  global $wu_user_info;
  
  // retrieve event-specific information from the server
  $rc = voip_event_get_event_from_nid($wu_voip_server, $wu_user_info, $extension_info['nid_contents']);
  
  if ($rc['error_code'] != VOIP_SUCCESS) {
  	return -1;
  }
  
  // play the event info
  $calling_extension_name = t('Main menu');
  $r = _whatsup_event_play($rc['event_info'], $calling_extension_name, TRUE);
eh_log("event play rc: $rc");
  switch ($r) {
    case -1:
      return -1;
    case 4: // skip to next event
    case 1: // success, move to next event
    case 2: // return to calling extension
    case 3: // go back to main menu
      return 1;
    default:
      eh_error("Processing error reading event from nid: $rc.");
      $prompt = v("Processing error. Please contact your system administrator.");
      vio_say($prompt);
      return -1;
  }
}

/*
 * Ask user for the event start and end dates and times
 * 
 * @param $start_only
 * boolean. If TRUE, does not ask the user to input the end date and time of the event. In this case, the result will be an event that start and end at same date and time.
 */

function _whatsup_event_get_dates_and_times($start_only = FALSE) {

  $prompt_date_start = v("Please enter the date your event begins. Use two digits for the month, two digits for the day and four digits for the year.");
  $prompt_date_end = v("Please enter the date your event ends. Use two digits for the month, two digits for the day and four digits for the year.");
  $invalid_date_msg = v("Invalid date. Please try again.");
  
  $prompt_time_start = v("Please enter the time your event starts. Use two digits for the hour and two digits for the minutes.");
  $prompt_time_end = v("Please enter the time your event ends. Use two digits for the hour and two digits for the minutes.");
  $invalid_time_msg = v("Invalid time. Please try again.");
  
  $choices_end_date = array(1 => v('Does your event end on the same day it starts? If so, press 1.'),
                            2 => v('To define a new end date, press 2.'));
  
  $prompt_cancel_txt = 'To cancel the creation of this event';   

  $valid_dates = FALSE;
  while (!$valid_dates) {
    // record the start date and time
    $start_date = _whatsup_get_date($prompt_date_start, $invalid_date_msg, $prompt_cancel_txt);  
    if (!is_array($start_date)) {
      return $start_date;
    }  
    $start_time = _whatsup_get_time($prompt_time_start, $invalid_time_msg, $prompt_cancel_txt);  
    if (!is_array($start_time)) {
      return $start_time;
    }  
    $timestamp_start = mktime($start_time['hour'], $start_time['minute'], $start_time['second'], $start_date['month'], $start_date['day'], $start_date['year']);  
    $array_start = array_merge($start_date, $start_time);

    if ($start_only) {
      $valid_end_date = TRUE;
      $timestamp_end = $timestamp_start;  
      $array_end = $array_start;
      $valid_dates = TRUE;
    }
    else {
      $valid_end_date = FALSE;
    }

    while (!$valid_end_date) {
     // record the end date
      $choice = vio_menu($choices_end_date);
      if ($choice == 1) {
        $end_date = $start_date;
      }
      else {
        // record the end date
        $end_date = _whatsup_get_date($prompt_date_end, $invalid_date_msg, $prompt_cancel_txt);  
        if (!is_array($end_date)) {
          return $end_date;
        }  
      } 

      // make sure end date happens after start date...
      $timestamp_start_date = mktime(0, 0, 0, $start_date['month'], $start_date['day'], $start_date['year']); 
      $timestamp_end_date = mktime(0, 0, 0, $end_date['month'], $end_date['day'], $end_date['year']); 
      if ($timestamp_start_date <= $timestamp_end_date) {
        $valid_end_date = TRUE;
      }
      else {
        $choices_invalid_end_date = array();
        $choices_invalid_end_date[1] = v('Your event end date is happening before the start date. To input a new end date, press 1.');
        $choices_invalid_end_date[2] = v('To record all the event dates and times once again, press 2.');
        $choice = vio_menu($choices_invalid_end_date);
        switch ($choice) {
          case 1: // re-record end date
            break;
          case 2: // re-record all dates and times
            $valid_end_date = TRUE;
            break;
          default:
            eh_error("Invalid choice selected when confirming invalid end event date: $choice");
            vio_say('Processing error. Please contact your system administrator.');
            return -1;
        }
      }

      $valid_end_time = FALSE;
      while ((!$valid_end_time) && $valid_end_date) {      
        // record the end time
        $end_time = _whatsup_get_time($prompt_time_end, $invalid_time_msg, $prompt_cancel_txt);  
        if (!is_array($end_time)) {
          return $end_time;
        }  
        $timestamp_end = mktime($end_time['hour'], $end_time['minute'], $end_time['second'], $end_date['month'], $end_date['day'], $end_date['year']);  
        $array_end = array_merge($end_date, $end_time);
        // if singleday event, make sure end time is not before start time
        if ($timestamp_end > $timestamp_start) {
          $valid_end_time = TRUE;
          $valid_dates = TRUE;
        }
        else {
          $choices_invalid_end_time = array();
          $choices_invalid_end_time[1] = t('Your event end time is happening before the start time. To record a new end time, press 1.');
          $choices_invalid_end_time[2] = t('To re-record all the event dates and times once again, press 2.');
          $choice = vio_menu($choices_invalid_end_time);
          switch ($choice) {
            case 1: // re-record end time
              break;
            case 2: // re-record all dates and times
              $valid_end_time = TRUE;
              break;
            default:
              eh_error("Invalid choice selected when confirming invalid end event date: $choice");
              vio_say('Processing error. Please contact your system administrator.');
              return -1;
          } 
        }
      } // while $valid_end_time
    } // while $valid_end_date
  } // while $valid_dates

  $start_format = ($timestamp_start % 3600) ? 'l, M j, Y @ g:i A' : 'l, M j, Y @ g A';
  if ($start_only) {
    $prompt = t('Your event is planned for %event_start', array('%event_start' => date($start_format, $timestamp_start)));
  }
  else {
    $end_format = ($timestamp_end % 3600) ? 'l, M j, Y @ g:i A' : 'l, M j, Y @ g A';
    $prompt = t('Your event is planned to start %event_start and to end %event_end.', 
                 array('%event_start' => date($start_format, $timestamp_start), '%event_end' => date($end_format, $timestamp_end)));
  }
eh_log("$prompt");
   vio_say($prompt);

  $rc = array('timestamp_start' => $timestamp_start, 'array_start' => $array_start, 'timestamp_end' => $timestamp_end, 'array_end' => $array_end);
  return $rc;   
}


/*
 * return -1 in case of failure, 0 in case of cancellation or, in case of success, an array with 'year', 'month', 'day' the input date
 */
function _whatsup_get_date($prompt_date, $prompt_invalid_date, $prompt_cancel_txt) {
  $format_date = 'mdY';
  $choices_confirm_input = array(1 => v('To accept this input, press 1.'),
                                 2 => v('To replay it, press 2.'),
                                 3 => v('To record it once again, press 3.'),
                                 '#' => v($prompt_cancel_txt, array('%key' => t('the pound key'))));

  // record the date
  $input_ok = FALSE;
  while (!$input_ok) {
    $date = vio_get_date($prompt_date, $prompt_invalid_date, $format_date);
eh_log('date: ' . serialize($date));
    $year = isset($date['year'])?$date['year']:0;
    $month = isset($date['month'])?$date['month']:0;
    $day = isset($date['day'])?$date['day']:0;
    $timestamp_date = mktime(0, 0, 0, $month, $day, $year);
    vio_say(t('You entered %date.', array('%date' => date('l, M j, Y', $timestamp_date))));
    $selecting_choices = TRUE;
    while ($selecting_choices) {
      $choice = vio_menu($choices_confirm_input);
      switch ($choice) {
        case 1:
          $selecting_choices = FALSE;
          $input_ok = TRUE;
          break;
        case 2:
          vio_say(v('You entered %date.', array('%date' => date('l, M j, Y', $timestamp_date))));
          break;
        case 3: 
          $selecting_choices = FALSE;
          break; // record the date once again
        case '#':
          vio_say(v('Operation cancelled.'));
          return 0;
        default: // invalid choice
          eh_error("Invalid choice selected when confirming date: $choice");
          vio_say(v('Processing error. Please contact your system administrator.'));
          return -1;
      }
    }
  }
  
  $rc = array('year' => $year, 'month' => $month, 'day' => $day);
  return $rc;  
}

/*
 * return -1 in case of failure, 0 in case of cancellation or, in case of success, an array with 'hour', 'minute', and 'second' of the input time
 */
function _whatsup_get_time($prompt_time, $prompt_invalid_time, $prompt_cancel) {
  // determine whether input time should be in 12h- or 24h-format
  global $wu_voip_server;
  $r = voip_event_get_ampm_flag($wu_voip_server); 
  if ($r['ampm_format']) {
    $format_time = 'hi';
    $is_am_msg = t('If this is an A M time, press 1.');
    $is_pm_msg = t('If this is a P M time, press 3.');
  }
  else {
    $format_time = 'Hi';
    $is_am_msg = NULL;
    $is_pm_msg = NULL;
  }
  
  $choices_confirm_input = array(1 => t('To accept this input, press 1.'),
                                 2 => t('To replay it, press 2.'),
                                 3 => t('To record it once again, press 3.'),
                                 '#' => t('%cancel_msg, press the pound key.', array('%cancel_msg' => $prompt_cancel)));

  // record the time
  $input_ok = FALSE;
  while (!$input_ok) {
    $time = vio_get_time($prompt_time, $prompt_invalid_time, $format_time, $is_am_msg, $is_pm_msg);
eh_log('time: ' . print_r($time, TRUE));
    $hour = isset($time['hour'])?$time['hour']:0;
    $minute = isset($time['minute'])?$time['minute']:0;
    $second = isset($time['second'])?$time['second']:0;
eh_log("calling mktime($hour, $minute, $second)");
    $timestamp_time = mktime($hour, $minute, $second);
eh_log("timestamp: $timestamp_time");
    $time_format = ($timestamp_time % 3600) ? 'g:i A' : 'g A';
//    vio_say(t('You entered %time.', array('%time' => date($time_format, $timestamp_time))));
    $prompt = t('You entered %time.', array('%time' => date($time_format, $timestamp_time)));
eh_log("prompt: $prompt");
    vio_say($prompt);
    $selecting_choices = TRUE;
    while ($selecting_choices) {
      $choice = vio_menu($choices_confirm_input);
      switch ($choice) {
        case 1:
          $selecting_choices = FALSE;
          $input_ok = TRUE;
          break;
        case 2: // replay the time
eh_log(t('You entered %time.', array('%time' => date($time_format, $timestamp_time))));
          vio_say(t('You entered %time.', array('%time' => date($time_format, $timestamp_time))));
          break;           
        case 3: // record the time once again
          $selecting_choices = FALSE;
          break; 
        case '#':
          vio_say(t('Operation cancelled.'));
          return 0;
        default: // invalid choice
          eh_error("Invalid choice selected when confirming input time: $choice");
          vio_say('Processing error. Please contact your system administrator.');
          return -1;
      }
    }
  }
  $rc = array('hour' => $hour, 'minute' => $minute, 'second' => $second);
  return $rc;  
}


/*
 * Return values: -1 :: processing error
 *                 1 :: success
 *                 2 :: cancel / go back to calling extension
 *                 3 :: cancel / go back to main menu (not implemented, yet)
 *                 4 :: skip / move to next event
 *                 5 :: back / move to previous event (not implemented, yet)
 */
function _whatsup_event_play($event_info, $calling_extension_name, $as_extension = FALSE) {
  global $wu_voip_server;
  global $wu_user_info;
eh_log('playing event_info: ' . serialize($event_info));  
 
  // determine the event organizer
  if (isset($event_info['organizer_extension_info']['file_audio_name'])) {
    $basename = $event_info['organizer_extension_info']['file_audio_name'];
    $organizer_name = "*$basename";
  }
  else if (isset($event_info['organizer_extension_info']['name'])) {
    $organizer_name = $event_info['organizer_extension_info']['name'];
  }
  else {
    $organizer_name = t('extension %n', array('%n' => $event_info['organizer_extension_info']['number']));
  }

  $organizer_extension_number = $event_info['organizer_extension_info']['number'];
  
  // determine the event title
  if (isset($event_info['extension_info']['file_audio_name'])) {
    $basename = $event_info['extension_info']['file_audio_name'];
    $event_title = "*$basename";
  }
  else if (isset($event_info['extension_info']['name'])) {
    $event_title = $event_info['extension_info']['name'];
  }
  else {
    $event_title = t('Event organized by extension %extension_number', array('%extension_number' => $organizer_extension_number));
  }
  
  // determine the event description and date
  $event_description = '';
  if (isset($event_info['extension_info']['file_audio_description'])) {
    $basename = $event_info['extension_info']['file_audio_description'];
    $event_description = "*$basename";
  }
  
  $event_date = _whatsup_friendly_date_and_time($event_info['ts_start']);

/******* 
  // determine the event location 
  if (isset($event_info['file_audio_location'])) {
    $basename = $event_info['file_audio_location'];
    $event_location = "*$basename";
  }
  else {
    $event_location = t('I am sorry, but there is no location recorded for this event.');
  }
********/

  // determine current event state
  $ts = time();
  if ($event_info['ts_start'] > $ts) {
    $state = 'not_started';
  }
  else if ($event_info['ts_end'] < $ts) {
    $state = 'ended';
  }
  else {
  	$state = 'ongoing';
  }

  // determine how to present the event status
  $start_date = _whatsup_friendly_date_and_time($event_info['ts_start']);
  $end_date = _whatsup_friendly_date_and_time($event_info['ts_end']);
  $time_format = ($event_info['ts_end'] % 3600) ? 'g:i A' : 'g A';
  $end_time_str = date($time_format, $event_info['ts_end']);
  switch ($state) {
    case 'not_started':
      if ($event_info['ts_start'] == $event_info['ts_end']) {
        $status_prompt = v('This is an event organized by %organizer_name of extension %organizer_extension. It will start %start_date.', 
        		           array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number, 
                           '%start_date' => $start_date));
      }
      else if ($event_info['kind'] == 'single_day') {
        $status_prompt = v('This is a single-day event organized by %organizer_name of extension %organizer_extension. '
        		           . ' It will start %start_date and will end at %end_time.', 
        		           array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number, 
                           '%start_date' => $start_date, '%end_time' => $end_time_str));
      }
      else {
        $status_prompt = v('This is a multi-day event organized by %organizer_name of extension %organizer_extension. '
      	                   . 'It will start %start_date and will end %end_date.', 
      		               array('%organizer_name' => $organizer_name,  '%organizer_extension' => $organizer_extension_number,
                                 '%start_date' => $start_date, '%end_date' => $end_date));
      }
      break;
    case 'ended':
      if ($event_info['ts_start'] == $event_info['ts_end']) {
        $status_prompt = v('This is an event organized by %organizer_name of extension %organizer_extension. It ended %end_date.', 
      		               array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number,
                                 '%end_date' => $end_date));
      }
      else if ($event_info['kind'] == 'single_day') {
        $status_prompt = v('This is a single-day event organized by %organizer_name of extension %organizer_extension. '
                            . ' It started %start_date and ended at %end_time.', 
                	        array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number, 
                                  '%start_date' => $start_date, '%end_time' => $end_time_str));
      }
      else {
        $status_prompt = v('This is a multi-day event organized by %organizer_name of extension %organizer_extension. '
      	                   . ' It started %start_date and ended %end_date.', 
      		               array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number,
                                 '%start_date' => $start_date, '%end_date' => $end_date));
      }
      break;
    case 'ongoing':
      if ($event_info['kind'] == 'single_day') {
        $status_prompt = v('This is a single-day event organized by %organizer_name of extension %organizer_extension. '
                            . ' It started %start_date and will end at %end_time.', 
                	        array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number, 
                                  '%start_date' => $start_date, '%end_time' => $end_time_str));
      }
      else {
        $status_prompt = v('This is a multi-day event organized by %organizer_name of extension %organizer_extension. '
                            . ' It started %start_date and will end %end_date.', 
     		                array('%organizer_name' => $organizer_name, '%organizer_extension' => $organizer_extension_number,
                                  '%start_date' => $start_date, '%end_date' => $end_date));
      }
      break;
  }
eh_log($status_prompt);

  // check if organizer has an active mailbox
  $has_active_mailbox = FALSE;  
  $rc = voip_voicemail_mailbox_get_info($wu_voip_server, $wu_user_info, $organizer_extension_number);
  if (($rc['error_code'] != VOIP_SUCCESS) && ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST)) {
    eh_error('Error retrieving mailbox info: ' . $rc['error_msg']);
    return -1;
  }
  if ($rc['error_code'] != VOIP_VOICEMAIL_MAILBOX_DOES_NOT_EXIST) {
    $mailbox_info = $rc['mailbox_info'];
    if ($mailbox_info['is_active']) {
      $has_active_mailbox = TRUE;
    }
  }

  // initialize user menus 
  
  $choices2 = array();
  $choices2[1] = v('To replay this event, press 1.');
//  $choices2[2] = v('For event location, press 2.');
  if ($has_active_mailbox) {
    $choices2[3] = v('To leave a voicemail message to the event organizers, press 3.');
  }
  if (!$as_extension) {
    $choices2[9] = v('To move to the next event, press 9');
  }
  $choices2['#'] = v('To go back to %extension_name, press the pound key', array('%extension_name' => $calling_extension_name));
eh_log('choices2: ' . serialize($choices2));
 
  $stop = FALSE;
  while (!$stop) {
    	
    // play the event information
    vio_say($event_title);
    vio_say($event_description, '#');
    vio_say($status_prompt);
    
    // update access statistics associated with the event just read
    $rc2 = @asa_update_audio_entry_statistics($wu_voip_server, $wu_user_info, $event_info['extension_info']['nid_contents']);
    if (!$rc2) {
eh_log("Error updating event statistics: " . eh_error_msg());
      vio_say("Processing error.  Please contact your system administrator.");
      return -1;
    }
    
    $stop_advanced_options = FALSE;
    while (!$stop_advanced_options) {    
      // present advanced options to user
      $choice = vio_menu($choices2);
        switch($choice) {
          default: // invalid option
          case -1: // processing error
            eh_error("Error selecting event menu options (choice: $choice)");
            $prompt = v('Processing error. Please contact your system administrator.');
            vio_say($prompt);
            return -1;
    
          case 1: // replay the event info once again
            $stop_advanced_options = TRUE;
            break;
/****        
          case 2: // play event location
            vio_say($event_location);
            break;
****/          
          case 3: // record a voicemail message to event organizers
            $destination_list = array($organizer_extension_number);
            $r = _whatsup_voicemail_message_record($destination_list, $organizer_extension_number);
            if ($r == -1) {
              return -1;
            }
            break;

        case '9': // go to the next event
          return 4;
        
        case '#': // go back to calling extension
          return 2;
        
      }
    }
  }
}

/*
 * Return -1 in case of processing failure, 1 in case of success
 *
 */
function _whatsup_event_management_area($organizer_extension_info, $is_organizer, $calling_extension_name) {
  global $wu_user_info;
  global $wu_voip_server;
  
  // determine the organizer's name
  if (isset($organizer_extension_info['file_audio_name'])) {
    $basename = $organizer_extension_info['file_audio_name'];
    $organizer_name = "*$basename";
  }
  else {
    $organizer_name = t('extension %n', array('%n' => $organizer_extension_info['number']));
  }
  
  $choices = array();
  $choices[1] = (!empty($organizer_extension_info['uid']))
                ? v('For a directory of upcoming events organized by this user, press 1.')
                : v('For a directory of upcoming group events, press 1.');
  $choices[2] = v('To replay a specific event, press 2.');
  $choices[3] = v('To create a new event, press 3.');
  $choices[4] = v('To delete a event, press 4.');
  $choices['#'] = v("To go back to %calling_extension_name, press the pound key.", array('%calling_extension_name' => $calling_extension_name));

  $stop = FALSE;
  while (!$stop) {
    // Welcome caller to the group's event management area
    vio_beep();
    $prompt = v('Event management area for %organizer_name', array('%organizer_name' => $organizer_name));
    vio_say($prompt);

    $choice = vio_menu($choices);
    switch($choice) {
      case 1: // check upcoming events organized by the given organizer
        // remind user to login
        $prompt_login_option = v('Only registered users have access to non-public events. To loggin, press 1.');
        $prompt_other_option = v('To remain anonymous, press the pound key.');
        $r = _whatsup_check_login($prompt_login_option, $prompt_other_option);
        if ($r == -1) {
          $rc = -1;
	      $stop = TRUE;
	      break;
        }
        $timestamp = time();
        $start = array();
        $start['year'] = date('Y', $timestamp);
        $start['month'] = date('n', $timestamp);
        $start['day'] = date('j', $timestamp);
        $start['hour'] = date('G', $timestamp);
        $start['minute'] = date('i', $timestamp);
        $start['second'] = date('s');
        $query_info['start'] = $start;
        $query_info['end'] = NULL;
        $query_info['limit'] = NULL;
        $query_info['terms'] = NULL;
        $query_info['organizer_extension_number'] = $organizer_extension_info['number'];
        $query_info['gid'] = isset($organizer_extension_info['uid'])?NULL:$organizer_extension_info['nid_contents'];
        $prompt_no_events = v('I am sorry, but there are no events currently planned for %group_name. Please try again later.', array('%group_name' => $organizer_name));
        $prompt_end_of_events = v('End of event listing.');
        $prompt_description = v('Upcoming events for %group_name', array('%group_name' => $organizer_name));
        $txt_calling_extension_name = t('Event management.');
        $r = _whatsup_event_query_and_play($wu_voip_server, $wu_user_info, $query_info, $prompt_no_events, $prompt_end_of_events, $prompt_description, $txt_calling_extension_name);
        switch ($r) {
	      default:
	      case WU_PROCESSING_FAILURE: // processing error
             vio_say(v("Processing error. Please contact your system administrator."));
	        $rc = -1;
	        $stop = TRUE;
	        break;
	              
	      case WU_GO_TO_MAIN_MENU: // cancel. go back to main menu
	        $rc = 0;
	        $stop = TRUE;
	        break;
	              
          case WU_GO_BACK: 
          case WU_SUCCESS: 
	        break;
	    }
        break;
        
      case 2: // event replay
        $r = _whatsup_event_review();
        if ($r == -1) {
          $rc = -1;
          $stop = TRUE;
        }
        break;
        
      case 3: // create new event
        $allow_public_events = $is_organizer;
        $r = _whatsup_event_create_event($organizer_extension_info, $allow_public_events);
        if ($r == -1) {
          $rc = -1;
          $stop = TRUE;
        }
        break;
           
      case 4: // delete group event
        $prompt = v('Please dial the extension number of the event you want to delete. When done, press the pound key.');
        $no_input_msg = v('No input received. Please try again.');
        $input = vio_get_input($prompt, 3, $no_input_msg);
        switch($input) {
	      case -1: // Input error
	        eh_error('Error reading input from the user.');
            $rc = -1;
            $stop = TRUE;
          case '': // user didn't type anything. go back
            eh_log('User did not press any extension.');
            $rc = 1;
            $stop = TRUE;
          default:
            $extension_number = $input;
            break;
        }
        // check if the desired extension is valid.
        $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $extension_number);
        if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
  	      eh_error('Processing error retrieving extension info: ' . $result['error_msg']);
  	      vio_say(t('Processing error. Please contact your system administrator.'));
          $rc = -1;
          $stop = TRUE;
        }
        else if ($result['error_code'] == VOIP_INVALID_EXTENSION_NUMBER) {
          eh_error("Invalid extension: {$result['error_msg']}");
          $prompt = t("%extension_number is not a valid extension number.", array('%extension_number' => $extension_number));
          vio_say($prompt);
          break;
        }
        else if ($result['error_code'] == VOIP_EXTENSION_DOES_NOT_EXIST) {
          eh_error("Invalid extension: {$result['error_msg']}");
          $prompt = t("I am sorry, but extension %extension_number does not exist.", array('%extension_number' => $extension_number));
          vio_say($prompt);
          break;
        }
        else {
          // check extension type  
          $event_extension_info =& $result['extension_info'];
          if (!isset($event_extension_info['type_contents']) || ($event_extension_info['type_contents'] != 'voip_event')) {
            $prompt = t("%extension_number is not a valid event extension number.", array('%extension_number' => $extension_number));
            vio_say($prompt);
            break;
          }

          // delete the event extension
          $choices_delete = array();
          $choices_delete[1] = v('Are you sure you want to delete this event? If so, press 1.');
          $choices_delete['#'] = v('To cancel this operation, press the pound key.');
          $choice_delete = vio_menu($choices_delete);
          switch ($choice_delete) {
            case -1:
              eh_error("Error selecting event delete option: $choice_delete");
              vio_say(v("Processing error. Please contact your system administrator."));
              $rc = -1;
              $stop = TRUE;
              break;

            case '#': 
              $prompt = v("Operation cancelled.");
              vio_say($prompt);
              break;

            case 1:
// TODO: make sure only group admins are allowed to delete a public event...
// TODO: note that we're not checking if the event being deleted is associated with the current group... how much of a problem is this?
              $r = voip_event_delete_event($wu_voip_server, $wu_user_info, $event_extension_info);
              switch ($r['error_code']) {
                case VOIP_SUCCESS:
                  vio_say(t('Event successfuly deleted.'));
                  break;
                case VOIP_LACK_OF_PERMISSION:
                  vio_say(t('Permission denied. Are you sure you typed in the right extension number?'));
                  break;
                case VOIP_PROCESSING_FAILURE:
                default:
                  vio_say(t('Processing error. Please contact your system administrator.'));
                  $rc = -1;
                  $stop = TRUE;
                  break;
              }
              break;
          } // switch
        } // else
        break;
            
      case '#':
        $stop = TRUE;
        $rc = 1;
        break;
            
      default:
        eh_error("Error selecting member management menu options (choice: $choice)");
        $prompt = v('Processing error. Please contact your system administrator.');
        vio_say($prompt);
        $rc = -1;
        $stop = TRUE;
        break;
          
    }        	
  }
  return $rc;
}


/*
 * Allow caller to browse the audioblog entries that obey the specified criteria.
 *
 *  * @param $query_info
 *   array with the query criteria for the audioblog entries:
 *     ['status'] :: 1 = published, 0 = unpublished, NULL = any
 *     ['uid'] :: user id of the desired entries' author, or NULL for any
 *     ['categories_or'] :: array with category ids to be ORed; or NULL for all
 *     ['categories_and'] :: array with category ids to be ANDed; or NULL for all
 *     ['unread_entries_only'] :: TRUE, return only the audioblog entries that haven't been read by the user
 *
 * @return
 *   integer -1 in case of error, 0 if user cancelled operation, 1 in case of success.
 *    In case of error, check eh_error_msg() for explanation.
 */
function _whatsup_audioblog_query_and_play($query_info, $no_audioblogs_msg = NULL) {
eh_log("Entering: _whatsup_audioblog_query_and_play: " . print_r($query_info, TRUE));

  global $wu_voip_server;
  global $wu_user_info;
  
  if (!$no_audioblogs_msg) {
    $no_audioblogs_msg = v("I am sorry, but there are no audioblog entries to be downloaded under the specified criteria.");
  }

  $result = asa_retrieve_meta_audio_entries($wu_voip_server, $wu_user_info, $query_info);
  if (!$result) { 
    return -1;
  }

  $count_entries = $result['count'];
  if ($count_entries == 0) {
eh_log("No audioblog entries to be downloaded");
    vio_say($no_audioblogs_msg);
    return 1;
  }

  $entries_tmp =& $result['entries'];

  // determine whether or not to differenciate between new and old audioblog entries for the user
  $present_status = (isset($wu_user_info['user_name'])) ? TRUE : FALSE;
     
  if (!$present_status) {
  	if ($count_entries > 1) {
      $prompt = v("There are %count audioblog entries to be checked.", array('%count' => $count_entries));
  	}
  	else {
      $prompt = v("There is only 1 audioblog entry to be checked.");
  	}
  	vio_say($prompt);
  	$entries =& $entries_tmp;
  }
  else {
    // separate old and new audioblog entries
    $new_entries = array();
    $old_entries = array();
    foreach ($entries_tmp as $index => $audioblog) {
      if ($audioblog['unread']) {
        $new_entries[] =& $entries_tmp[$index];
      }
      else {
        $old_entries[] =& $entries_tmp[$index];
      }
    }

    // tell caller the number of old and new audioblog entries
    $count_new_entries = count($new_entries);
    $count_old_entries = count($old_entries);

    $prompt = 'You have ';
    if ($count_new_entries) {
      $prompt .= ($count_new_entries == 1) ? '1 new audioblog entry ' : "%count_new_entries new audioblog entries ";
    }

    if ($count_old_entries) {
      if ($count_new_entries) {
        $prompt .= 'and ';
      }
      $prompt .= ($count_old_entries == 1) ? '1 old audioblog entry ' : "%count_old_entries old audioblog entries ";
    }
    $prompt .=  'to listen to.';
eh_log($prompt);
    vio_say(v($prompt, array('%count_new_entries' => $count_new_entries, '%count_old_entries' => $count_old_entries)));
    
    // determine which audioblog entries to present
    if (($new_entries && !$old_entries) || (!$new_entries && $old_entries)) {
      $entries =& $entries_tmp;
    }
    else {
      // ask caller which audioblog entries to present
      $choices = array();
      $choices[1] = v('To listen to the new ' . (($count_new_entries == 1)? 'entry' : 'entries') . ', press 1.');
      $choices[2] = v('To listen to the old ' . (($count_old_entries == 1)? 'entry' : 'entries') . ", press 2.");
      $choices[3] = v('To listen to all the audioblog entries retrieved, press 3.');
      $choices['#'] = v('To go back to the main menu, press the pound key.');
      $choice = vio_menu($choices);

      switch ($choice) {
        case 1: // play new audioblog entries
          $entries =& $new_entries;
          break;
        case 2: // play old audioblog entries
          $entries =& $old_entries;
          break;
        case 3: // play all audioblog entries
          $entries =& $entries_tmp; // use the list already retrieved
          break;
        case '#': // return to main menu;
          $prompt = v('Returning to main menu.');
          vio_say($prompt);
          return 0;
        default: // invalid option
          eh_error('Invalid option selected on _whatsup_audioblog_query_and_play');
          return -1;
      }
    }
  }

  // Play the selected audioblog entries to the caller
  $r = _whatsup_audioblog_play_audioblog_list($entries, $present_status);

  return $r;

}

/*
 * Return 1 in case of success and -1 in case of processing failure
 */
function _whatsup_audioblog_play_audioblog_list($entries, $present_status = FALSE) {
  $browse_entries = TRUE;
  $index = 0;
  while ($browse_entries) {

    vio_say(v("Entry number %i", array('%i' => ($index + 1))));

    $audioblog_info = $entries[$index];

    $list_navigation = TRUE;
eh_log("calling:  _whatsup_audioblog_play($audioblog_info, $list_navigation, $present_status);");
    $rc = _whatsup_audioblog_play($audioblog_info, $list_navigation, $present_status);

    switch ($rc) {

      case 2: // stop playing audioblog entries
        $browse_entries = FALSE;
        break;
  
      case 1: // success; move on to next entry
      case 3:  // skip to next entry
        if ($index >= count($entries) - 1) {
          $prompt = v('No more audioblog entries to be read.');
          vio_say($prompt);
          $browse_entries = FALSE; // return to main menu
        }
        else {
          // move to the next entry
          $index++;
        }
        break;

      case 4: // go to the previous entry
        if ($index == 0) {
          $prompt = v("This is the first audioblog entry.  There are no more entries before this one.");
          vio_say($prompt);
        }
        else {
          // move to the previous entry
          $index--;
        }
        break;

      case 5: // entry has been deleted
        unset($entries[$index]);
        $num_entries = count($entries);
        if ($num_entries == 0) {
          $browse_entries = FALSE;
        }
        else {  // reorganize indexes of the array
          $entries = array_values($entries);
          if ($index >= count($entries)) {
            vio_say("No more audioblog entries to be read.");
            $browse_entries = FALSE;
          }
        }
        break;
        
      case -1:
        return -1;
  
      default:
        eh_error("Invalid value returned from playing audioblog entries: $rc");
        vio_say(v("Processing error. Please contact your system administrator."));
        return -1;
    }
  }

  return 1;

}

// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------


/*
 * Allows caller to select a group of categories associated with audioblog entries
 * 
 * @param $category_list
 * array with category names and ids
 *
 * @param $accept_none
 *   boolean. If TRUE, allows caller to select the option of entries that do not have any category associated with them.
 *
 * @param $accept_any
 *   boolean.  If TRUE, allows caller to select the option 'any category' (which also includes entries with no category).
 *
 * @return
 *   object containing boolean success and a selected_ids array with the ids (tids) of the selected categories.
 */
function _whatsup_get_categories($category_list, $accept_none = FALSE, $accept_multiple = TRUE, $accept_any = FALSE, $accept_any_txt = NULL) {
eh_log("_whatsup_get_categories($category_list, $accept_none, $accept_multiple, $accept_any, $accept_any_txt)");
//TODO: allow category names to be recorded
  if (!$accept_any_txt) {
    $accept_an_txt = "To select entries from any of the existing categories, press %key";
  }
    
  $result = new stdClass();
  $selected_ids = array();
  $index_any = NULL;
  $index_none = NULL;

  // if no categories defined,
  if (!$category_list) {
    $prompt = v("I am sorry, but there are no categories defined in the system.");
    vio_say($prompt);
    $result->success = TRUE;
    $result->selected_ids = $selected_ids;   // return an empty array
    return $result;
  }

  $choices = array();
  $category_table = array();
  $index = 1;

  foreach ($category_list as $category) {
eh_log( "category name: " . $category['category_name'] . ", id: " . $category['category_id'] );
    $category_table[$index]['category_name'] = $category['category_name'];
    $category_table[$index]['category_id'] = $category['category_id'];
    $prompt = v("For %category_name press %key", array('%category_name' => t($category['category_name']), '%key' => t($index)));
    $choices[$index] = $prompt;
    $index++;
  }

  // add an option for the 'any' category
  if ($accept_any) {
    $index_any = $index;
    $category_table[$index]['category_name'] = 'Any category';
    $category_table[$index]['category_id'] = 'Any';
    $choices[$index] = v($accept_any_txt, array('%key' => $index));;
    $index++;
  }

  // add an option for the 'none' category
  if ($accept_none) {
    $index_none = $index;
    $category_table[$index]['category_name'] = 'No categories';
    $category_table[$index]['category_id'] = 'None';
    $prompt = v("For 'no category', press %key", array('%key' => $index));
    $choices[$index] = $prompt;
  }

  // create the secondary menus
  $choices2 = array();
  $choices2['1'] = v('To hear your current category selection, press 1.');
  $choices2['2'] = v('To accept the current selection, press 2.');
  if ($accept_multiple) {
    $choices2['3'] = v('To add more categories to your selection, press 3.');
    $choices2['4'] = v('To restart selecting categories from the beginning, press 4.');
  }
  else {
     $choices2['4'] = v('To select a different category, press 4.');
  }
  
  $choices3 = array();
  $choices3['2'] = v('To accept the current selection, press 2.');
  $choices3['3'] = v('To make a different selection, press 3.');


  // loop until done
  $stop = FALSE;
  $start_prompt = v('Please select the desired category.');
  while (!$stop) {
    vio_say($start_prompt, '#');
 
    $selected_choices = array();
    $selected_ids = array();
    $choices_tmp = $choices;

    $read_selection = TRUE;
    while ($read_selection) {

      // read the categories as a menu for the user
      $choice = vio_menu($choices_tmp);

      switch ($choice) {
        case -1:
          eh_error("Error selecting category options");
          return $result;

        case $index_any:
          $read_selection = FALSE;
          $selected_ids = array(NULL);
          $prompt = v("You selected: any category.");
          break;

        case $index_none:
          $read_selection = FALSE;
          $selected_ids = NULL;
          $prompt = v("You selected: no categories.");
          break;

        default:
          $selected_choices[] = $choice;
          $selected_ids[] = $category_table[$choice]['category_id'];       // annotate the selection
          unset($choices_tmp[$choice]); // remove the selected option from the ones available
          $prompt = v('You selected: %category.', array('%category' => $category_table[$choice]['category_name']));
      }
      
      vio_say($prompt);
      
      // if either 'any' or 'none' selected
      if (($selected_ids === NULL) || ($selected_ids === array(NULL))) {
        $choices_confirm =& $choices3;
      }
      else {
      	$choices_confirm =& $choices2;
      }
      	
      $menu2 = TRUE;
      while ($menu2) {
        $choice2 = vio_menu($choices_confirm);
        switch ($choice2) {
          case 1: // replay current selection
            if (count($selected_choices) == 0) {
              $selection = t('no category selected');
            }
            else {
              $index = 0;
              $c = $selected_choices[$index];
              $selection = $category_table[$c]['category_name'];
              $index++;
              while ($index < count($selected_choices) - 1) {
                $c = $selected_choices[$index];
                $selection .= ', ' . $category_table[$c]['category_name'];
                $index++;
              }
              if ($index < count($selected_choices)) { // add an 'and' before the last category is read
                $c = $selected_choices[$index];
                $selection .= t(' and ') . $category_table[$c]['category_name'];
              }
              $prompt .= '.';
            } 
            // read the final selection back to the caller
            vio_say(v('Your current selection is: %selection.', array('%selection' => $selection)), '#');
            break;
          case 2: // accept current selection
            $result->success = TRUE;
            $result->selected_ids = $selected_ids;
            $menu2 = FALSE;
            $read_selection = FALSE;
            $stop = true;
            break;
          case 3: // keep adding categories to the selection
            $menu2 = FALSE;
            break;
          case 4: // restart selection from the beginning
            $menu2 = FALSE;
            $read_selection = FALSE;
            break;
          default: // processing error
            eh_error("Error selecting options");
            $result->success = FALSE;
            $menu2 = FALSE;
            $read_selection = FALSE;
            $stop = true;
            break;
        } // switch $choice2
      } // while $menu2
    } // read $selection
  } // while !$stop

eh_log("result from select categories: " . print_r($result, TRUE));
  return $result;
}




/*
 * Let caller handle advanced options associated with the specified audioblog entry
 *
 * Return values: -1 :: processing error
 *                 1 :: success
 *                 2 :: user-pressed stop 
 *                 3 :: skip / move to next entry
 *                 4 :: back / move to previous entry
 *                 5 :: entry deleted
 */
function _whatsup_audioblog_play($audioblog_info, $list_navigation = FALSE, $present_status = FALSE) {
eh_log("_whatsup_audioblog_play($audioblog_info, $list_navigation = FALSE, $present_status = FALSE)");
eh_log('audioblog_play: ' . print_r($audioblog_info, TRUE));

  global $wu_voip_server;
  global $wu_user_info;
 
  $char_prev = ($list_navigation)?'7':'';  // move to next entry
  $char_next = ($list_navigation)?'9':''; // move to previous entry
  $char_advanced_options = '*'; // go to the audioblog entry menu
  $char_stop = '#'; // stop playing 
  $escape_digits = " $char_prev $char_next $char_advanced_options $char_stop";


  // make sure the entry can be played
  if (!$audioblog_info['file_info']) {
    vio_say("I am sorry, but this entry did not have an audio file associated with it");
    return 1;
  }

  // Retrieve file associated with the entry from either the server or local cache
  $basename_wav = asa_get_audio_file($wu_voip_server, $wu_user_info, array('nid' => $audioblog_info['nid']));
  if ($basename_wav == -1) {
    vio_say(v("Processing error. Please contact your system administrator."));
    return -1;
  }
  else if ($basename_wav === 0) {
    vio_say(v("I am sorry, but you do not have permission to access the specified audioblog entry."));
    return 1;
  }

  // check if the caller has the necessary permissions to delete the entry
  $r = asa_check_access($wu_voip_server, $wu_user_info, 'delete', $audioblog_info['nid']);
  if ($r['error_code'] == -1) {
    vio_say(v("Processing error. Please contact your system administrator."));
    return -1;
  }
  $allow_delete = $r['allowed'];
eh_log("allow_delete: $allow_delete");
  
  // build the advanced options menu for the entry
  $choices = array();
  $choices[1] = v("To replay this audioblog entry, press 1.");
  $choices[2] = v("To hear additional information about this entry, press 2.");
  if ($allow_delete) {
    $choices[3] = v("To delete the current audioblog entry, press 3.");
  }
  $choices['#'] = v('To go back, press the pound key.');
  if ($list_navigation) {
    $choices[$char_prev] = v('To move to the previous entry, press %key', array('%key' => $char_prev));
    $choices[$char_next] = v('To move to the next entry, press %key', array('%key' => $char_next));
    $choices['#'] = v("To stop playing audioblog entries, press the pound key.");
  }  
  
  
  $stop_processing = FALSE;
  while (!$stop_processing) {

    if ($present_status && $audioblog_info['unread']) {
      vio_say(v("This is a new entry."));
    }

    // Read the file back to the caller
    $rc = vio_control_stream_file($basename_wav, $escape_digits);
    if (!$rc) {
      vio_say(v("Processing error. Please contact your system administrator."));
      return -1;
    }
    
    // update access statistics associated with the entry just read
    $rc2 = @asa_update_audio_entry_statistics($wu_voip_server, $wu_user_info, $audioblog_info['nid']);
    if (!$rc2) {
eh_log("Error updating audioblog entry statistics: " . eh_error_msg());
      vio_say(v("Processing error. Please contact your system administrator."));
      return -1;
    }

    // process user key stroke
    switch ($rc['key_pressed']) {
      default:
        eh_error("Invalid key pressed while playing audiblog entry");
        vio_say(v("Processing error. Please contact your system administrator."));
        return -1;

      case $char_stop:
        return 2;

      case $char_next:
        return 3;

      case $char_prev: // go to the previous entry
        return 4;

      case NULL:  // entry read until its end
      case $char_advanced_options:  // process advanced-options menu
        break;
    }	

    $advanced_menu = TRUE;
    while ($advanced_menu) {
      // present the advanced options menu
      $choice = vio_menu($choices);

      switch ($choice) {
        case 1: // replay the entry
          continue 3; // go back 3 looping structures...
        
        case 2: // read envelope information
          $text =  "Audioblog title: %title. ";
//        $size =  (!empty($audioblog['file_info'])) ? $audioblog['file_info']['size'] : NULL;
//        $text .=  ($size) ? sprintf("Size: %.1f kilobytes. ", (float)($audioblog_info['file_info']['size'] / 1024)) : '';
          $playtime =  ($audioblog_info['file_info']) ? $audioblog_info['file_info']['playtime'] : NULL;
          $text .=  ($playtime) ? ('Duration: %play_time. ') : '';
//        $text .= "Node i d number " . $audioblog_info['nid'] . ". " ;
          $text .= "Last modified on %date ";
          // determine the name of the user who recorded the entry
          $user_name = NULL;
          $extension_number = NULL;
          $uid = $audioblog_info['drupal_uid'];
          $r = voip_extension_get_info_from_uid($wu_voip_server, $wu_user_info, $uid);
          if ($r['error_code'] == VOIP_SUCCESS) {
            $user_name = (isset($r['extension_info']['file_audio_name']))?('*' . $r['extension_info']['file_audio_name']):t('user');
            $extension_number = t('extension %x', array('%x' => $r['extension_info']['number']));
            $text .= "by %user_name from %extension_number.";
          }
          $text = strip_tags($text);
eh_log("$text");
          vio_say(v($text, array('%title' => $audioblog_info['title'], 
                                 '%play_time' => _whatsup_playtime_from_string($playtime),
                                 '%date' => date('l, M j, Y @ g:i A', $audioblog_info['last_modified']),
                                 '%user_name' => $user_name, '%extension_number' => $extension_number)), '#');
          break;

        case 3: // delete entry
          $choices_delete = array();
          $choices_delete[1] = v('Are you sure you want to delete this entry? If so, press 1.');
          $choices_delete['#'] = v('To cancel this operation, press the pound key.');
          $choice_delete = vio_menu($choices_delete);
          switch ($choice_delete) {
            case -1:
              eh_error("Error selecting choice delete option: $choice_delete");
              vio_say(v("Processing error. Please contact your system administrator."));
              return -1;

            case '#': 
              $prompt = v("Operation cancelled.");
              vio_say($prompt);
              break;

            case 1:
eh_log("Delete entry");
              $result = asa_delete_audio_entry($wu_voip_server, $wu_user_info, $audioblog_info);
              switch ($result) {
                case -1: // error deleting entry
                  vio_say(v("Processing error. Please contact your system administrator."));
                  return -1;
                case 1: // entry has been deleted successfully
                  vio_say(v("Audioblog entry deleted."));
                  return 5;
                case 2:
                default: // entry could not be deleted, perhaps due to lack of user permissions
                  $prompt = v("Audioblog entry could not be deleted. Are you sure you have the necessary permissions to delete this entry?");
                  vio_say($prompt);
                  break;
              }
            break;
          }

        case $char_stop:
           return 2;

        case $char_next:
          return 3;

        case $char_prev: // go to the previous entry
          return 4;
 
        default: // invalid option selected
          eh_error("Error selecting entry advanced options: $choice");
          return -1;
      } // switch
    } // while advanced_menu
  } // while
}


/*
 * Performs a user login
 * 
 * @return
 *   integer -1 in case of processing failure, 0 in case of unsuccessful login. In case of success, returns 1 and sets
 *   the global $wu_user_info with the name, password and extension number of the current user 
 */
function _whatsup_login() {
  global $wu_user_info;
  global $wu_voip_server;
  
  // Check if user is already logged in...
  if (isset($wu_user_info['user_name'])) {
  	return 1;
  }

  // get user id and password
  $count = 0;
  $max_tries = 3;
  $stop = FALSE;

  while (!$stop) {
  	
    vio_beep();
    $prompt = v("System loggin. Please type in your personal extension number. When done, press the pound key.");
    $extension_number = vio_get_input($prompt);
    if ($extension_number == -1) {
      eh_error('Error reading extension number from user.');
      $prompt = v("Processing error. Please contact your system administrator.");
      vio_say($prompt);
      return -1;
    }
    if ($extension_number == '') {
      $prompt = v("No input received. Please try again");
      vio_say($prompt);
      continue;
    }
eh_log("new extension number: " . $extension_number);

    $prompt = v("Please type in your password. When done, press the pound key.");
    $phone_pin = vio_get_input($prompt);
    if ($phone_pin == -1) {
      eh_error('Error reading password from user.');
      $prompt = v("Processing error. Please contact your system administrator.");
      vio_say($prompt);
      return -1;
    }
    else if ($phone_pin == '') {
      $prompt = v("No input received. Please try again");
      vio_say($prompt);
      continue;
    }
eh_log("new phone_pin: " . $phone_pin);

    // get the user info
    $result = voip_user_get_info($wu_voip_server, $extension_number, $phone_pin);
    
    if ($result['error_code'] == VOIP_SUCCESS) {

      $wu_user_info['user_name'] = $result['user_info']['user_name'];
      $wu_user_info['password'] = $result['user_info']['password'];
      vt_set_voice($result['user_info']['voice_id']);
       
      // now get the user extension info
      $result = voip_extension_get_info($wu_voip_server, $wu_user_info, $extension_number);
      
      if ($result['error_code'] == VOIP_SUCCESS) {
      	
      	// add extension number to global variable
      	$wu_user_info['extension_number'] = $result['extension_info']['number'];
        $prompt = v("Loggin successful.");
        vio_say($prompt);
        voip_log($wu_voip_server, $wu_user_info, 'user', 'logged in');
        return 1;
      }
    }
    
    $wu_user_info['user_name'] = NULL;
    
    if ($result['error_code'] == VOIP_PROCESSING_FAILURE) {
      eh_log($result['error_msg']);
      return -1;
    }
    
    // go for the next round    
    $count++;
    if ($count == $max_tries) {
      eh_log('Maximum attempts to login reached.');
      $prompt = v('Invalid extension number and password combination.  Please contact your system administrator.');     
      vio_say($prompt);
      return 0;
    }

    // let the caller try again
    $prompt = v('Invalid loggin.  Please try again.');
    vio_say($prompt);
    voip_log($wu_voip_server, $wu_user_info, 'user', 'login failure');
    continue;
  }
}

/*
 * Check if the user is logged in and, if not, play the prompt and provide the option to log in
 * 
 * @return
 *   int -1 in case of failure, 0 in case of no login, 1 in case of successful login
 */
function _whatsup_check_login($prompt_login_option, $prompt_other_option) {
  global $wu_user_info;
  // Make sure user is logged in
  if (!empty($wu_user_info['user_name'])) {
  	$rc = 1;
  }
  else {
  	$choices = array();
  	$choices[1] = $prompt_login_option;
  	$choices['#'] = $prompt_other_option;
  	$choice = vio_menu($choices);
  	switch ($choice) {
      default: // invalid option
      case -1: // processing error
        eh_error("Error selecting check_login options (choice: $choice)");
        vio_say(v('Processing error. Please contact your system administrator.'));
        $rc = -1;
      case 1:
        $rc = _whatsup_login();
        break;
      case '#':
        $rc = 0;
        break;
  	}
  }
  return $rc;
}
 
/*
 * Changes the user's existing phone pin
 * 
 * @return
 *   integer -1 in case of processing failure, 0 in case of invalid attempt, 1 in case of success 
 */
function _whatsup_change_phone_pin() {
  global $wu_user_info;
  global $wu_voip_server;
  
  // get user id and password
  $c_tries = 0;
  $max_tries = 3;
  $stop = FALSE;

  while (!$stop && ($c_tries < $max_tries)) {
  	
    vio_beep();
    $prompt = v("Please type your current password.");
    $old_pin = vio_get_input($prompt);
    if ($old_pin == -1) {
      eh_error('Error reading current phone pin from user.');
      vio_say(v("Processing error. Please contact your system administrator."));
      return -1;
    }
    if ($old_pin == '') {
      vio_say(v("No input received. Please try again"));
      $c_tries++;
      continue;
    }

    $prompt = v("Please type in the new password. When done, press the pound key.");
    $new_pin = vio_get_input($prompt);
    if ($new_pin == -1) {
      eh_error('Error reading new phone pin from user.');
      $prompt = v("Processing error. Please contact your system administrator.");
      vio_say($prompt);
      return -1;
    }
    else if ($new_pin == '') {
      $prompt = v("No input received. Please try again");
      vio_say($prompt);
      $c_tries++;
      continue;
    }

    $prompt = v("Please retype the new password. When done, press the pound key.");
    $new_pin_confirmation = vio_get_input($prompt);
    if ($new_pin_confirmation == -1) {
      eh_error('Error reading new phone pin from user.');
      $prompt = v("Processing error. Please contact your system administrator.");
      vio_say($prompt);
      return -1;
    }
    else if ($new_pin_confirmation == '') {
      $prompt = v("No input received. Please try again.");
      vio_say($prompt);
      $c_tries++;
      continue;
    }
    else if ($new_pin != $new_pin_confirmation) {
      $prompt = v("Passwords don't match. Please try again.");
      vio_say($prompt);
      $c_tries++;
      continue;
    }

    // get the user info
    $result = voip_change_phone_pin($wu_voip_server, $wu_user_info, $old_pin, $new_pin);
    
    if ($result['error_code'] == VOIP_SUCCESS) {
      vio_say(v('Your phone password has been successfully updated.'));
      return 1;
    }
    else if ($result['error_code'] == VOIP_INVALID_PHONE_PIN) {
      vio_say(v('Password provided does not match existing one.  Please try again'));
      $c_tries++;
    }
    else { 
      vio_say(v('Processing error. Please contact your system administrator.'));
      eh_error('Processing error trying to change phone pin: error code = ' . $result['error_code']);
      return -1;
    }
  }
  
  if ($c_tries > $max_tries) {
    eh_log('Maximum attempts to login reached.');
    vio_say(v('Invalid extension number and password combination.  Please contact your system administrator.'));     
    voip_log($wu_voip_server, $wu_user_info, 'voip', 'failed to change phone pin', NULL, NULL);
    return 0;
  } 
}


/*
 * Create a formatted post (title and body) using templates and input data
 * 
 * @param
 *   object input data
 * @return
 *   object formatted post (title and body)
 */
function render_output($template_file, $replacements) {
eh_log("render_output: template_file: $template_file");
 
  extract($replacements);        // Extract the vars to local namespace
  ob_start();                    // Start output buffering
  include($template_file);       // Include the file
  $contents = ob_get_contents(); // Get the contents of the buffer
  ob_end_clean();                // End buffering and discard
eh_log("render_output: contents: $contents");
  return $contents;              // Return the contents
}  

/*
 * Return a string with the equivalent hours/minutes/seconds derived from the given playtime string.
 */
function _whatsup_playtime_from_string( $playtime_string) {
  $time_seconds = 0;
  $factor = 1;
  $vals = explode(':', $playtime_string);
  for ($i = count($vals); $i > 0; $i--) {
    $time_seconds += array_pop($vals) * $factor;
    $factor *= 60;
  }
  $hour = floor($time_seconds/3600);
  $minute = floor(($time_seconds - ($hour * 3600))/60);
  $second = $time_seconds - (($hour * 3600)+($minute * 60));

  $result = ($hour) ? (($hour == 1) ? '1 hour ' : "$hour hours ") : '';
  $result .= ($minute && !$second) ? 'and ' : '';
  $result .= ($minute) ? (($minute == 1) ? '1 minute ' : "$minute minutes ") : '';
  $result .= (($hour || $minute) && $second) ? 'and ' : '';
  $result .= ($second) ? (($second == 1) ? '1 second' : "$second seconds ") : '';

  return $result;
}

/*
 * Read text input from the telephone keypad
 */
function _whatsup_get_text($prompt, $cancel_text = NULL, $max_tries = 3, $max_digits = NULL, $timeout = NULL) {
  $choices = array();
  $choices[1] = ''; // save this slot
  $choices[3] = v('To re-enter your text, press 3');
  if ($cancel_text) {
    $choices['#'] = v($cancel_text, array('%key' => t('the pound key')));
  }

  $read_text = TRUE;
  $c_tries = 0;
  while ($read_text && ($c_tries < $max_tries)) {  
    $text = vio_get_text($prompt, $max_digits, $timeout);
    if ($text == -1) {
      eh_error('Processing error reading text input from the telephone.');
      vio_say(v('Processing error. Please contact your system administrator.'));
      return -1;
    }
  
    $count = strlen($text);
    if (!$count) {
      vio_say(v('No input received. Please try again.'));
      $c_tries++;
    }
    else {
      // there's no need to replay the input if its only 1 character long
      if (is_null($max_digits) || ($max_digits > 1)) {
        vio_say(v('Your current selection is: '));
        vio_say_digits($text);
      }
      $choices[1] = v('To accept this input, press 1.');
      $choice = vio_menu($choices);
      switch ($choice) {
        case 1:
          return $text;
        case 3:
          break;
        case '#': // cancel operation
          return NULL;
      }
    }
  } 
}

function _whatsup_select_voice() {
  $menu = TRUE;
  while ($menu) {

    // retrieve available voices from the database
    $voices = vt_get_voices();
    if ($voices === FALSE) {
      return FALSE;
    }
eh_log("voices: " . print_r($voices, TRUE));    
    // create a voice-selection menu
    $voice_choices = array();
    foreach ($voices as $index => $voice_entry) {
      $voice_choices[$index + 1] = v('For %voice_name, type %voice_id.', array('%voice_name' => ('*' . $voice_entry['file_basename']), '%voice_id' => ($index+1)));
    }
    // play voice options to caller
    vio_beep();
    vio_say(t('Voice selection menu.'));
    if ($voices) {
      vio_say(v("Please select the voice you would like to use."));
    }
    $choice = vio_menu($voice_choices);
    switch ($choice) {
      case -1:
        eh_error("Error selecting voice options (choice: $choice)");
        vio_say(v('Processing error. Please contact your system administrator.'));
        $rc = FALSE;
        $menu = FALSE;
        break;

      default: // voice selected
        global $wu_voip_server;
        global $wu_user_info;
        $voice_id = $voices[$choice - 1]['vid'];
        $r = voip_change_voice_id($wu_voip_server, $wu_user_info, $voice_id); 
        if ($r['error_code'] !== VOIP_SUCCESS) {
          $rc = FALSE;
        }
        else {
          $rc = vt_set_voice($voice_id);
        }
        if ($rc) {    
          vio_say(v("System voice changed to %voice_name", array('%voice_name' => '*' . $voices[$choice - 1]['file_basename'])));
        }
        $menu = FALSE;
        break;
    }
  }
eh_log("rc: $rc");  
  return $rc;
}


?>
