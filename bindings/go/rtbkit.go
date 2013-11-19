package main

// #cgo LDFLAGS: -L../../../build/x86_64/bin -lrtbkit_api -Wl,-rpath,../../../build/x86_64/bin
// #include <stdlib.h>
// #include "../../testing/rtbkit_api.h"
import "C"
import "unsafe"
import "fmt"

type BiddingAgentHandler interface {
    OnBidRequest()
}

type Handle struct {
    handle *C.struct_rtbkit_handle
    biddingAgentHandlers map[*C.struct_rtbkit_handle]BiddingAgentHandler
}

type Object struct {
    handle *C.struct_rtbkit_object
}

func NewHandle(bootstrap string) *Handle {
    cs := C.CString(bootstrap)
    item := new(Handle)
    item.handle = C.rtbkit_initialize(cs)
    C.free(unsafe.Pointer(cs))
    return item;
}

func Shutdown(handle *Handle) {
    C.rtbkit_shutdown(handle.handle);
}

func NewBiddingAgent(handle *Handle, name string) *Object {
    cs := C.CString(name)
    item := new(Object)
    item.handle = C.rtbkit_create_bidding_agent(handle.handle, cs)
    C.free(unsafe.Pointer(cs))
    return item;
}

func Release(handle *Object) {
    C.rtbkit_release(handle.handle)
}

func NextEvent(handle *Handle) *Event {
    return new(Event)
}

func main() {
    fmt.Println("creating rtbkit")
    var handle = NewHandle("../../sample.bootstrap.json")
    var agent = NewBiddingAgent(handle, "test-agent")
    Release(agent)
    Shutdown(handle)
}

