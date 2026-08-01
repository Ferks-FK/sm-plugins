Automatically records demos based on the number of players on the server (with additional support for uploading demos to an external site/panel).

New Convars
------
- `sm_autorecord_upload_enable` - Enable automatic demo upload after recording stops. Default: `"1"`
- `sm_autorecord_upload_url` - The URL of your site/panel to send demo files. Default: `""`
- `sm_autorecord_upload_apikey` - The ApiKey to autenticate the upload with site/panel. Default: `""`
- `sm_autorecord_recover_enable` - On startup, scan the demo folder for leftover `.dem` files (e.g. left behind by a crash or manual restart) and upload them. Requires `sm_autorecord_pathfmt` to use `%m` as its own path segment (e.g. `"demos/%m/%l"`), since that's the only way to know which match a leftover demo belongs to. Default: `"1"`

New Natives
------
- `AR_GetMatchID` - Get the current match id. 

Original: https://github.com/shqke/sp_public/tree/master/autorecorder