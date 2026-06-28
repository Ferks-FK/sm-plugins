Automatically records demos based on the number of players on the server (with additional support for uploading demos to an external site/panel).

New Convars
------
- `sm_autorecord_upload_enable` - Enable automatic demo upload after recording stops. Default: `"1"`
- `sm_autorecord_upload_url` - The URL of your site/panel to send demo files. Default: `""`
- `sm_autorecord_upload_apikey` - The ApiKey to autenticate the upload with site/panel. Default: `""`

New Natives
------
- `AR_GetMatchID` - Get the current match id. 

Original: https://github.com/shqke/sp_public/tree/master/autorecorder