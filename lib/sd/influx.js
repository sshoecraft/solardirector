
/*
Copyright (c) 2022, Stephen P. Shoecraft
All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree.
*/

function influx_dump_results(results) {
        for(i=0; i < results.length; i++) {
                var series = results[i];
                var columns = series.columns;
        //      dprintf(0,"columns.length: %d\n", columns.length);
                for(j=0; j < columns.length; j++) printf("column[%d]: %s\n", j, columns[j]);
                var values = series.values;
        //      dprintf(0,"values.length: %d\n", values.length);
                for(j=0; j < values.length; j++) {
                        for(k=0; k < columns.length; k++) {
        //                      printf("values[%d][%d] type: %s\n", j,k,typeof(values[j][k]));
                                printf("values[%d][%d]: %s\n", j,k,values[j][k]);
                        }
                }
        }
}
