import QtQuick 2.0

QtObject {
    property int index_size_err: DOMException.INDEX_SIZE_ERR
    property int domstring_size_err: DOMException.DOMSTRING_SIZE_ERR
    property int hierarchy_request_err: DOMException.HIERARCHY_REQUEST_ERR
    property int wrong_document_err: DOMException.WRONG_DOCUMENT_ERR
    property int invalid_character_err: DOMException.INVALID_CHARACTER_ERR
    property int no_data_allowed_err: DOMException.NO_DATA_ALLOWED_ERR
    property int no_modification_allowed_err: DOMException.NO_MODIFICATION_ALLOWED_ERR
    property int not_found_err: DOMException.NOT_FOUND_ERR
    property int not_supported_err: DOMException.NOT_SUPPORTED_ERR
    property int inuse_attribute_err: DOMException.INUSE_ATTRIBUTE_ERR
    property int invalid_state_err: DOMException.INVALID_STATE_ERR
    property int syntax_err: DOMException.SYNTAX_ERR
    property int invalid_modification_err: DOMException.INVALID_MODIFICATION_ERR
    property int namespace_err: DOMException.NAMESPACE_ERR
    property int invalid_access_err: DOMException.INVALID_ACCESS_ERR
    property int validation_err: DOMException.VALIDATION_ERR
    property int type_mismatch_err: DOMException.TYPE_MISMATCH_ERR

    Component.onCompleted: {
        // Attempt to overwrite and delete values
        DOMException.INDEX_SIZE_ERR = 44;
        DOMException.DOMSTRING_SIZE_ERR = 44;
        DOMException.HIERARCHY_REQUEST_ERR = 44;
        DOMException.WRONG_DOCUMENT_ERR = 44;
        DOMException.INVALID_CHARACTER_ERR = 44;
        DOMException.NO_DATA_ALLOWED_ERR = 44;
        DOMException.NO_MODIFICATION_ALLOWED_ERR = 44;
        DOMException.NOT_FOUND_ERR = 44;
        DOMException.NOT_SUPPORTED_ERR = 44;
        DOMException.INUSE_ATTRIBUTE_ERR = 44;
        DOMException.INVALID_STATE_ERR = 44;
        DOMException.SYNTAX_ERR = 44;
        DOMException.INVALID_MODIFICATION_ERR = 44;
        DOMException.NAMESPACE_ERR = 44;
        DOMException.INVALID_ACCESS_ERR = 44;
        DOMException.VALIDATION_ERR = 44;
        DOMException.TYPE_MISMATCH_ERR = 44;

        delete DOMException.INDEX_SIZE_ERR;
        delete DOMException.DOMSTRING_SIZE_ERR;
        delete DOMException.HIERARCHY_REQUEST_ERR;
        delete DOMException.WRONG_DOCUMENT_ERR;
        delete DOMException.INVALID_CHARACTER_ERR;
        delete DOMException.NO_DATA_ALLOWED_ERR;
        delete DOMException.NO_MODIFICATION_ALLOWED_ERR;
        delete DOMException.NOT_FOUND_ERR;
        delete DOMException.NOT_SUPPORTED_ERR;
        delete DOMException.INUSE_ATTRIBUTE_ERR;
        delete DOMException.INVALID_STATE_ERR;
        delete DOMException.SYNTAX_ERR;
        delete DOMException.INVALID_MODIFICATION_ERR;
        delete DOMException.NAMESPACE_ERR;
        delete DOMException.INVALID_ACCESS_ERR;
        delete DOMException.VALIDATION_ERR;
        delete DOMException.TYPE_MISMATCH_ERR;
    }
}
