if (process.platform !== 'win32') {
  console.log('Non-Windows platform detected, skipping build.');
  process.exit(0);
}

