<?php
if (!isset($_GET["id"])) exit("
<meta name='viewport' content='width=device-width, height=device-height'>
<div style='
    display: flex;
    flex-direction: column;
    zoom: 1.5;
    text-align: center;
'>
<h4>Give `id` param pls.</h4>
<form method='get'><input name='id'><input type='submit'>
<br><br>{$_SERVER['REQUEST_URI']}?id={mod.id}
");
$id = $_GET["id"];

$url = "https://api.geode-sdk.org/v1/mods/$id/logo";
if (@file_get_contents($url)) {
    header('Location: '.$url); exit(); 
}

$url = "https://api.geode-sdk.org/v1/mods/$id";
$content = file_get_contents($url);
$array = json_decode($content, 1);

$mod = $array["payload"];

$source = array_key_exists("source", is_array($mod["links"]) ? $mod["links"] : array()) ? @$mod["links"]["source"] : "";
$source = empty($source) ? @$mod["repository"] : $source;
if (!empty($source)) {
    
    if (strstr($source, "github.com")) {
        
        $repo = "";
        $last_peace = ""; $bam = 0;
        foreach (explode("/", $source) as $peace) {
            if ($last_peace == "github.com" or $bam == 1) {
                $repo .= ($bam == 1 ? "/" : "")."$peace";
                $bam += 1;
            }
            $last_peace = $peace;
        }
        
        $raw = "https://raw.githubusercontent.com/$repo";
        
        $url = $raw."/".$mod["versions"][0]["version"]."/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/v".$mod["versions"][0]["version"]."/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/main/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/master/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
    }
    
    //https://gitlab.com/thebearodactyl/gay-wave-trail/-/raw/main/logo.png?ref_type=heads&inline=false
    if (strstr($source, "gitlab.com")) {
        
        $repo = "";
        $last_peace = ""; $bam = 0;
        foreach (explode("/", $source) as $peace) {
            if ($last_peace == "gitlab.com" or $bam == 1) {
                $repo .= ($bam == 1 ? "/" : "")."$peace";
                $bam += 1;
            }
            $last_peace = $peace;
        }
        
        $raw = "https://gitlab.com/$repo/-/raw";
        
        $url = $raw."/".$mod["versions"][0]["version"]."/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/v".$mod["versions"][0]["version"]."/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/main/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
        $url = $raw."/master/logo.png";
        if (@file_get_contents($url)) {
            header('Location: '.$url); exit(); 
        }
        
    }
}

http_response_code(404);