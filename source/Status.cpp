#include "Status.hpp"
namespace http
{
    std::string default_status_msg(StatusCode sc)
    {
        switch (sc)
        {
        case SC_CONTINUE: return "Continue";
        case SC_SWITCHING_PROTOCOLS: return "Switching Protocols";
        case SC_PROCESSING: return "Processing";

        case SC_OK: return "OK";
        case SC_CREATED: return "Created";
        case SC_ACCEPTED: return "Accepted";
        case SC_NON_AUTHORITATIVE_INFORMATION: return "Non-Authoritative Information";
        case SC_NO_CONTENT: return "No Content";
        case SC_RESET_CONTENT: return "Reset Content";
        case SC_PARTIAL_CONTENT: return "Partial Content";
        case SC_MULTI_STATUS: return "Multi-Status";
        case SC_ALREADY_REPORTED: return "Already Reported";
        case SC_IM_USED: return "IM Used";

        case SC_MULTIPLE_CHOICES: return "Multiple Choices";
        case SC_MOVED_PERMANENTLY: return "Moved Permanently";
        case SC_FOUND: return "Found";
        case SC_SEE_OTHER: return "See Other";
        case SC_NOT_MODIFIED: return "Not Modified";
        case SC_USE_PROXY: return "Use Proxy";
        case SC_TEMPORARY_REDIRECT: return "Temporary Redirect";
        case SC_PERMANENT_REDIRECT: return "Permanent Redirect";

        case SC_BAD_REQUEST: return "Bad Request";
        case SC_UNAUTHORIZED: return "Unauthorized";
        case SC_PAYMENT_REQUIRED: return "Payment Required";
        case SC_FORBIDDEN: return "Forbidden";
        case SC_NOT_FOUND: return "Not Found";
        case SC_METHOD_NOT_ALLOWED: return "Method Not Allowed";
        case SC_NOT_ACCEPTABLE: return "Not Acceptable";
        case SC_PROXY_AUTHENTICATION_REQUIRED: return "Proxy Authentication Required";
        case SC_REQUEST_TIMEOUT: return "Request Timeout";
        case SC_CONFIICT: return "Conflict";
        case SC_GONE: return "Gone";
        case SC_LENGTH_REQUIRED: return "Length Required";
        case SC_PRECONDITION_FAILED: return "Precondition Failed";
        case SC_PAYLOAD_TOO_LARGE: return "Payload Too Large";
        case SC_URI_TOO_LONG: return "URI Too Long";
        case SC_UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
        case SC_RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
        case SC_EXPECTATION_FAILED: return "Expectation Failed";
        case SC_MISDIRECTED_REQUEST: return "Misdirected Request";
        case SC_UNPROCESSABLE_ENTITY: return "Unprocessable Entity";
        case SC_LOCKED: return "Locked";
        case SC_FAILED_DEPENDENCY: return "Failed Dependency";
        case SC_UPGRADE_REQUIRED: return "Upgrade Required";
        case SC_PRECONDITION_REQUIRED: return "Precondition Required";
        case SC_TOO_MANY_REQUESTS: return "Too Many Requests";
        case SC_REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
        case SC_UNAVAILABLE_FOR_LEGAL_REASONS: return "Unavailable For Legal Reasons";

        case SC_INTERNAL_SERVER_ERROR: return "Internal Server Error";
        case SC_NOT_IMPLEMENTED: return "Not Implemented";
        case SC_BAD_GATEWAY: return "Bad Gateway";
        case SC_SERVICE_UNAVAILABLE: return "Service Unavailable";
        case SC_GATEWAY_TIMEOUT: return "Gateway Timeout";
        case SC_HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
        case SC_VARIANT_ALSO_NEGOTIATES: return "Variant Also Negotiates";
        case SC_INSUFFICIENT_STORAGE: return "Insufficient Storage";
        case SC_LOOP_DETECTED: return "Loop Detected";
        case SC_NOT_EXTENDED: return "Not Extended";
        case SC_NETWORK_AUTHENTICATION_REQUIRED: return "Network Authentication Required";

        default: return "Unknown";
        }
    }
}
