#if defined _autorecorder_included
  #endinput
#endif
#define _autorecorder_included

/**
 * Retrieves the current match ID.
 *
 * @param buffer     Buffer to store the match ID.
 * @param maxlength  Maximum length of the buffer.
 * @return           True if a match ID is currently set, false otherwise.
 */
native bool AR_GetMatchID(char[] buffer, int maxlength);

public SharedPlugin __pl_autorecorder =
{
    name = "autorecorder",
    file = "autorecorder.smx",
#if defined REQUIRE_PLUGIN
    required = 1,
#else
    required = 0,
#endif
};

#if !defined REQUIRE_PLUGIN
public void __pl_autorecorder_SetNTVOptional()
{
    MarkNativeAsOptional("AR_GetMatchID");
}
#endif
