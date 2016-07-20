#pragma once
#include <string>
namespace http
{
    //http://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml
    //2016-03-01

    enum StatusCode
    {
        SC_CONTINUE = 100,
        SC_SWITCHING_PROTOCOLS = 101,
        SC_PROCESSING = 102,

        SC_OK = 200,
        SC_CREATED = 201,
        SC_ACCEPTED = 202,
        SC_NON_AUTHORITATIVE_INFORMATION = 203,
        SC_NO_CONTENT = 204,
        SC_RESET_CONTENT = 205,
        SC_PARTIAL_CONTENT = 206,
        SC_MULTI_STATUS = 207,
        SC_ALREADY_REPORTED = 208,
        SC_IM_USED = 226,

        SC_MULTIPLE_CHOICES = 300,
        SC_MOVED_PERMANENTLY = 301,
        SC_FOUND = 302,
        SC_SEE_OTHER = 303,
        SC_NOT_MODIFIED = 304,
        SC_USE_PROXY = 305,
        SC_TEMPORARY_REDIRECT = 307,
        SC_PERMANENT_REDIRECT = 308,

        SC_BAD_REQUEST = 400,
        SC_UNAUTHORIZED = 401,
        SC_PAYMENT_REQUIRED = 402,
        SC_FORBIDDEN = 403,
        SC_NOT_FOUND = 404,
        SC_METHOD_NOT_ALLOWED = 405,
        SC_NOT_ACCEPTABLE = 406,
        SC_PROXY_AUTHENTICATION_REQUIRED = 407,
        SC_REQUEST_TIMEOUT = 408,
        SC_CONFIICT = 409,
        SC_GONE = 410,
        SC_LENGTH_REQUIRED = 411,
        SC_PRECONDITION_FAILED = 412,
        SC_PAYLOAD_TOO_LARGE = 413,
        SC_URI_TOO_LONG = 414,
        SC_UNSUPPORTED_MEDIA_TYPE = 415,
        SC_RANGE_NOT_SATISFIABLE = 416,
        SC_EXPECTATION_FAILED = 417,
        SC_MISDIRECTED_REQUEST = 421,
        SC_UNPROCESSABLE_ENTITY = 422,
        SC_LOCKED = 423,
        SC_FAILED_DEPENDENCY = 424,
        SC_UPGRADE_REQUIRED = 426,
        SC_PRECONDITION_REQUIRED = 428,
        SC_TOO_MANY_REQUESTS = 429,
        SC_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
        SC_UNAVAILABLE_FOR_LEGAL_REASONS = 451,

        SC_INTERNAL_SERVER_ERROR = 500,
        SC_NOT_IMPLEMENTED = 501,
        SC_BAD_GATEWAY = 502,
        SC_SERVICE_UNAVAILABLE = 503,
        SC_GATEWAY_TIMEOUT = 504,
        SC_HTTP_VERSION_NOT_SUPPORTED = 505,
        SC_VARIANT_ALSO_NEGOTIATES = 506,
        SC_INSUFFICIENT_STORAGE = 507,
        SC_LOOP_DETECTED = 508,
        SC_NOT_EXTENDED = 510,
        SC_NETWORK_AUTHENTICATION_REQUIRED = 511
    };

    std::string default_status_msg(StatusCode sc);

    struct Status
    {
        StatusCode code;
        std::string msg;
    };
}
