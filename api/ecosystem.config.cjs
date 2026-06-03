// PM2 process definition for thomaz-api on the Lightsail VM.
// cwd is the deployed path; env is loaded by Node via --env-file=.env.
module.exports = {
  apps: [
    {
      name: 'thomaz-api',
      script: 'dist/index.js',
      cwd: '/home/bitnami/apps/thomaz/api',
      node_args: '--env-file=.env',
      instances: 1,
      exec_mode: 'fork',
      max_memory_restart: '300M',
      autorestart: true,
    },
  ],
}
