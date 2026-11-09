function onStart(context) {
    host.status(host.assetText("note.txt"));
    host.request("light", context.params.brightness);
}
function onRequestResult(context, result) {
    if (result.accepted) host.complete();
}
