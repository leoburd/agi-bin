#!/usr/bin/php -q
<?php

foreach (array('voip_api.inc') as $file) {
  require_once(dirname(__FILE__) . DIRECTORY_SEPARATOR . $file); 
}

$voip_server = 'http://localhost/d6/xmlrpc.php';
#$voip_server = 'http://localhost/voipdev/xmlrpc.php';

// Internal functions

function asterisk_script_handler($server, $input_script_name, $input_options=array()) {
echo("Entering: asterisk_script_handler($server, $input_script_name, $input_options)\n");

  $processing = TRUE;
  $rc = array($status = TRUE);
  $script_name = $input_script_name;
  $options = $input_options;

  while($processing){

    // retrieve script
    $request_id = 'voip_get_script';
    $options['script_name'] = $script_name;
echo("about to call voip_process_request($server, $request_id, " . print_r($options,TRUE) . ")\n");
    $result = voip_process_request($server, $request_id, $options);
echo('result: ' . print_r($result, TRUE) . "\n");

    // check for processing errors
    if(voip_api_error()){
      $processing = FALSE;
      $rc['status'] = FALSE;
      $error_msg = "(asterisk script handler): Error retrieving $script_name from $server: " 
        . voip_error_message();
echo($error_msg . "\n");
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
echo("processing tag: " . $element['tag'] . "\n");
        switch($element['tag']) {
          default:
            // vio_say($element['tag'];
            break;

          case "GOTO":
            $script_name = $element['value'];
            $stop = TRUE;
            break;

          case "HANGUP":
            // hangup command
echo("about to hangup\n");
            $processing = FALSE;
            $stop = TRUE;
            break;

          case "SAY":
            $i++;
            $element = $values[$i];
            if($element['tag'] == 'TEXT') {
              // call vio_say($element['value']);
echo("say text: " . $element['value'] . "\n");
            } else {
              // call vio_play($element['value']);
echo("say file: " . $element['value'] . "\n");
            }
            break;
        }
      }
    }

    // reached the end of the script
    if($i == count($values)){
echo("end of the script\n");
      $processing = FALSE;
    }
  }

  // return
  return $rc;
}


echo("\n");
echo("-------\n");
echo("Testing xmlrpc infrastructure \n");
echo("-------\n");

echo("about to call system.listMethods\n");
$result = xmlrpc($voip_server, 'system.listMethods');
echo('result: ' . print_r($result, TRUE) . "\n");



echo("\n");
echo("-------\n");
echo("Testing asterisk_script_handler()\n");
echo("-------\n");

$request_id = 'voip_get_script';
echo("about to call voip_process_request($request_id)\n");
$options['script_name'] = 'hello_world';
$result = voip_process_request($voip_server, $request_id, $options);
echo('voip_api_error: ' . voip_api_error() . "\n");
echo('voip_api_error_message: ' . voip_api_error_message() . "\n");
echo('result: ' . print_r($result, TRUE) . "\n\n");


$script_name = 'hello_world';
$options = array('user_name' => 'Jon');
echo("about to call asterisk_script_handler($voip_server, $script_name, $options)\n");
$result = asterisk_script_handler($voip_server,$script_name, $options);
echo('result: ' . print_r($result, TRUE) . "\n\n");


echo("\n");
echo("-------\n");
echo("End of tests\n");
echo("-------\n");

?>
