
/* From Duane McCully: https://stackoverflow.com/questions/3118582/how-do-i-find-the-current-system-timezone */

/* doesnt support windows (yet) */

#include <string.h>
#include <strings.h>
#include <stdio.h>

char *get_tzname(char *tz, size_t tzSize);
static char *getValue(char *filename, char *tag, char *value, size_t valueSize);

/* Look for tag=someValue within filename.  When found, return someValue
 * in the provided value parameter up to valueSize in length.  If someValue
 * is enclosed in quotes, remove them. */
static char *getValue(char *filename, char *tag, char *value, size_t valueSize)
{
  char buffer[128], *lasts;
  int foundTag = 0;

  FILE *fd = fopen(filename, "r");
  if (fd)
  {
    /* Process the file, line by line. */
    while (fgets(buffer, sizeof(buffer), fd))
    {
      lasts = buffer;
      /* Look for lines with tag=value. */
      char *token = strtok_r(lasts, "=", &lasts);
      /* Is this the tag we are looking for? */
      if (token && !strcmp(token, tag))
      {
        /* Parse out the value. */
        char *zone = strtok_r(lasts, " \t\n", &lasts);
        /* If everything looks good, copy it to our return var. */
        if (zone && strlen(zone) > 0)
        {
          int i = 0;
          int j = 0;
          char quote = 0x00;
          /* Rather than just simple copy, remove quotes while we copy. */
          for (i = 0; i < strlen(zone) && i < valueSize - 1; i++)
          {
            /* Start quote. */
            if (quote == 0x00 && zone[i] == '"')
              quote = zone[i];
            /* End quote. */
            else if (quote != 0x00 && quote == zone[i])
              quote = 0x00;
            /* Copy bytes. */
            else
            {
              value[j] = zone[i];
              j++;
            }
          }
          value[j] = 0x00;
          foundTag = 1;
        }
        break;
      }
    }
    fclose(fd);
  }
  if (foundTag)
    return value;
  return NULL;
}

char *get_tzname(char *tz, size_t tzSize)
{
  char *ret = NULL;
  /* If there is an /etc/timezone file, then we expect it to contain
   * nothing except the timezone. */
  FILE *fd = fopen("/etc/timezone", "r"); /* Ubuntu. */
  if (fd)
  {
    char buffer[128];
    /* There should only be one line, in this case. */
    while (fgets(buffer, sizeof(buffer), fd))
    {
      char *lasts = buffer;
      /* We don't want a line feed on the end. */
      char *tag = strtok_r(lasts, " \t\n", &lasts);
      /* Idiot check. */
      if (tag && strlen(tag) > 0 && tag[0] != '#')
      {
        strncpy(tz, tag, tzSize);
        ret = tz;
      }
    }
    fclose(fd);
  }
  else if (getValue("/etc/sysconfig/clock", "ZONE", tz, tzSize)) /* Redhat.    */
    ret = tz;
  else if (getValue("/etc/TIMEZONE", "TZ", tz, tzSize))     /* Solaris. */
    ret = tz;
  return ret;
}
