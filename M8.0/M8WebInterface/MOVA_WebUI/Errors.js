"use strict";
function MOVAErrorsDictionary(err) {

    // Javascript enums equivalent
    var ErrorIDs = {
        MOVA_BACK_ON_LINE: 1,
        FAULTY_DETECTOR: 2,
        STAGE_NOT_ENDED: 3,
        INTERGREEN_NOT_ENDED: 4,
        INVALID_STAGE_DEMANDED: 5,
        WRONG_STAGE_CONFIRMED: 6,
        STAGE_STUCK_ON: 7,
        WATCHDOG_ROUTINE: 8,
        CONTROLLER_READY_BIT_OFF: 9,
        CONTROLLER_READY_BIT_ON: 10,
        PROGRAM_ERROR: 11,
        CHECKSUM_ERROR: 12,
        RELOADING_ERROR: 13,
        MULTIPLE_STAGE_CONFIRMS: 14,
        MOVA_RESTARTED: 15,
        DIVIDE_ERROR: 16,
        EMPTY_REPOSITORY_AREA: 17,
        REPOSITORY_CHECKSUM: 18,
        DEFAULT_LOADED: 19,
        ERROR_IDS_NUM: 20
    };
    return GetErrorText(err, ErrorIDs);

    function GetErrorText(err,ErrorIDs) {

        var errorID = err.ErrorID;
        var errorDate = new Date(err.DateTime).toUTCString(); 
        var add1 = err.Add1;
        var add2 = err.Add2;
        var add3 = err.Add3;

        switch (errorID) {
            case ErrorIDs.MOVA_BACK_ON_LINE:
                return String.format('MOVA BACK ON-LINE AT {0} ERR={1}',errorDate, add1);
                break;
            case ErrorIDs.FAULTY_DETECTOR:
                return String.format('FAULTY DETECTOR NO {0} AT {1}',add1, errorDate);
                break;
            case ErrorIDs.STAGE_NOT_ENDED:
                return String.format('STAGE NOT ENDED, S{0}-{1} OFF-LINE {2} ERR={3}',add1, add2, errorDate, add3);
                break;
            case ErrorIDs.INTERGREEN_NOT_ENDED:
                return String.format('30-SEC IG FAILS TO END. OFF-LINE AT {0} ERR={1}',errorDate, add1);
                break;
            case ErrorIDs.INVALID_STAGE_DEMANDED:
                return String.format('CHANGE S{0} S{1} NOT ALLOWED AT {2} ERR= {3}',add1, add2, errorDate, add3);
                break;
            case ErrorIDs.WRONG_STAGE_CONFIRMED:
                return String.format('WRONG STAGE SNO={0} DEM={1} AT {2}  ERR={3} ',add1, add2, errorDate, add3);
                break;
            case ErrorIDs.STAGE_STUCK_ON:
                return String.format('STAGE {0} STUCK FOR {1} MIN {2} ERR={3} ',add1, add2, errorDate, add3);
                break;
            case ErrorIDs.WATCHDOG_ROUTINE:
                return String.format('MOVA WATCHDOG AT {0}  ERR={1}  IO(18)={2}',errorDate, add1, add2);
                break;
            case ErrorIDs.CONTROLLER_READY_BIT_OFF:
                return String.format('Controller-ready bit OFF at {0}  ERR={1} ',errorDate, add1);
                break;
            case ErrorIDs.CONTROLLER_READY_BIT_ON:
                return String.format('Controller-ready bit ON at {0}  ERR={1} ',errorDate, add1);
                break;
            case ErrorIDs.PROGRAM_ERROR:
                return String.format('PROGRAM ERROR AT {0}  CODE={1}  RVL={2} ',errorDate, add1, add2);
                break;
            case ErrorIDs.CHECKSUM_ERROR:
                return String.format('CHECKSUM ERROR ON RE-LOADING SITE DATA AT {0} ',errorDate);
                break;
            case ErrorIDs.RELOADING_ERROR:
                return String.format('DATA #{0}; ERROR ON RE-LOADING AT {1} ',add1, errorDate);
                break;
            case ErrorIDs.MULTIPLE_STAGE_CONFIRMS:
                return String.format('MULTIPLE STAGE CONFIRMATIONS - MOVA STOPPED AT {0} ',errorDate);
                break;
            case ErrorIDs.MOVA_RESTARTED:
                return String.format('MOVA RESTARTED AT {0} ',errorDate);
                break;
            case ErrorIDs.DIVIDE_ERROR:
                return String.format('PROGRAM DIVIDE ERROR AT %(errorDate)  Location =%(add1) ',errorDate, add1);
                break;
            case ErrorIDs.EMPTY_REPOSITORY_AREA:
                return String.format('REPOSITORY AREA #%(add1); EMPTY ERROR AT %(errorDate) ',add1, errorDate);
                break;
            case ErrorIDs.REPOSITORY_CHECKSUM:
                return String.format('REPOSITORY AREA #{0}; CHECKSUM ERROR AT {1} ',add1, errorDate);
                break;
            case ErrorIDs.DEFAULT_LOADED:
                return String.format('REPOSITORY AREA #{0}; DEFAULT DATASET LOADED AT {1} ',add1, errorDate);
                break;
            default:
                return 'Unrecognised error by MOVA Tools.';
                break;
        }
    }
}


