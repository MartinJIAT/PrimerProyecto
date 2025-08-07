document.addEventListener('DOMContentLoaded', () => {
    const tempEl = document.getElementById('temp');
    const humidityEl = document.getElementById('humidity');
    const themeToggle = document.getElementById('theme-toggle');
  
    // Load saved data
    const savedTemp = localStorage.getItem('temperature');
    const savedHumidity = localStorage.getItem('humidity');
    const savedTheme = localStorage.getItem('theme');
  
    if (savedTemp) tempEl.textContent = `${savedTemp} °C`;
    if (savedHumidity) humidityEl.textContent = `${savedHumidity} %`;
    if (savedTheme === 'dark') {
      document.body.classList.add('dark');
      themeToggle.checked = true;
    }
  
    // Simulate fetching sensor data from ESP32
    setInterval(() => {
      // Simulated values — replace with actual fetch from ESP32
      const temp = (20 + Math.random() * 10).toFixed(1);
      const humidity = (40 + Math.random() * 20).toFixed(1);
  
      // Update DOM
      tempEl.textContent = `${temp} °C`;
      humidityEl.textContent = `${humidity} %`;
  
      // Store in localStorage
      localStorage.setItem('temperature', temp);
      localStorage.setItem('humidity', humidity);
    }, 5000); // Every 5 seconds
  
    // Theme toggle
    themeToggle.addEventListener('change', () => {
      document.body.classList.toggle('dark');
      localStorage.setItem('theme', document.body.classList.contains('dark') ? 'dark' : 'light');
    });
  });
  