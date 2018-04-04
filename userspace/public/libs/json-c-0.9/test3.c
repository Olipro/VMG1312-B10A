#include <stdio.h>
#include <stdlib.h>

#include "json.h"

int
main()
{
        struct json_object *jobj;
        char *input = (char*)malloc(1024);

        while (fgets(input, 1024, stdin) != NULL) {
                jobj = json_tokener_parse(input);
                if (is_error(jobj)) {
                        printf("error parsing json: %s\n",
			       json_tokener_errors[-(unsigned long)jobj]);
                } else {
                        printf("%s\n", json_object_to_json_string(jobj));
                        json_object_put(jobj);
                }
        }
        return 0;
}
