function onStart(context) {
    host.request("movement", 0);
    host.complete(); // Aggregate success still awaits the native measured-arrival outcome.
}
