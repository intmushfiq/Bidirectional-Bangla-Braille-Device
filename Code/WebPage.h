#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;


// =====================================================
// Webpage
// =====================================================

void handleRoot()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="bn">

<head>

<meta charset="UTF-8">

<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>বাংলা ব্রেইল অনুবাদক</title>

<style>

body {
    font-family: Arial, sans-serif;
    text-align: center;
    margin: 30px 15px;
}

h2 {
    font-size: 32px;
    margin-bottom: 30px;
}

textarea {
    width: 90%;
    height: 150px;

    padding: 15px;

    font-family: Arial, sans-serif;
    font-size: 24px;

    border: 2px solid #999;
    border-radius: 10px;

    resize: none;
}

button {
    width: 90%;

    margin-top: 20px;

    padding: 15px;

    font-size: 24px;
    font-weight: bold;

    border: none;
    border-radius: 10px;

    background: #222;
    color: white;
}

</style>

</head>

<body>

<h2>বাংলা ব্রেইল অনুবাদক</h2>

<form action="/send" method="GET">

<textarea
    name="text"
    placeholder="বাংলা লিখুন"
    autofocus
></textarea>

<br>

<button type="submit">
    অনুবাদ করুন
</button>

</form>

</body>

</html>
)rawliteral";

    server.send(200, "text/html; charset=UTF-8", html);
}

#endif