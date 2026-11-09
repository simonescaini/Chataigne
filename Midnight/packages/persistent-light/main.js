function onStart(context) {
    host.status("Listening to the explicitly live sensor alias");
    host.timer("heartbeat", 1, "scene");
}
function onInput(context, alias, value, token) {
    if (alias == "sensor") host.request("light", value > context.params.threshold ? context.params.brightness : 0);
}
function onTimer(context, id) {
    host.status("Scene clock heartbeat at " + context.sceneTime);
    host.timer("heartbeat", 1, "scene");
}
function onStop(context, reason) {
    // Host cleanup already closed admission. No output reset is implied.
}
