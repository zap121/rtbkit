var ref = require('ref');
var ffi = require('ffi');
var structure = require('ref-struct');

//var rtbkit_handle = ref.refType(ref.types.void);
//var rtbkit_object = ref.refType(ref.types.void);

var rtbkit_event = structure({
    'type': 'int',
    'subject': 'pointer'
});

var rtbkit_event_ptr = ref.refType(rtbkit_event);

var rtbkit = ffi.Library('librtbkit_api', {
    'rtbkit_initialize': [ 'pointer', [ 'string' ] ],
    'rtbkit_shutdown': [ 'void', [ 'pointer' ] ],
    'rtbkit_create_bidding_agent': [ 'pointer', [ 'pointer', 'string' ] ],
    'rtbkit_release': [ 'void', [ 'pointer' ] ],
    'rtbkit_next_event': [ 'void', [ 'pointer', rtbkit_event_ptr ] ],
    'rtbkit_free_event': [ 'void', [ 'pointer' ] ],
    'rtbkit_send_event': [ 'void', [ 'pointer' ] ],
    'rtbkit_fd': [ 'int', [ 'pointer' ] ]
});

var rtbkit_handle = rtbkit.rtbkit_initialize("sample.bootstrap.json");

var rtbkit_object = rtbkit.rtbkit_create_bidding_agent(rtbkit_handle, "test-agent");

var next = ref.alloc('int');
//rtbkit.rtbkit_next_event(rtbkit_object, next);

rtbkit.rtbkit_next_event.async(rtbkit_object, next, function (err, res) {
    if(err) throw err;
    console.log("next_event returned " + res);
});

rtbkit.rtbkit_release(rtbkit_object);

rtbkit.rtbkit_shutdown(rtbkit_handle);

