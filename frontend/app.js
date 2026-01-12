async function planFlight(){
  const start = performance.now();
  const resultElement = document.getElementById('result');
  const loadingElement = document.getElementById('loading');
  
  const input = {
    source: document.getElementById('source').value.toUpperCase(),
    destination: document.getElementById('destination').value.toUpperCase(),
    departure_date: document.getElementById('departure_date').value,
    departure_time: document.getElementById('departure_time').value + ":00",
    search_mode: document.getElementById("mode").value,
    adults:1
  };
  if(!input.source||!input.destination||!input.departure_date){
    alert("Please fill in all fields");
    return;
  }

  loadingElement.style.display = 'block';
  resultElement.innerHTML = '';
  try {
    const response = await fetch ('http://127.0.0.1:8000/plan', {
      method: 'POST',
      headers: {
        'Content-Type':'application/json'
      },
      body: JSON.stringify(input)
      });
      if(!response.ok){
        throw new Error(`Server error: ${response.statusText}`);}

      const data = await response.json();
      if(data.flights && data.flights.length>0){
        const header = document.createElement('h2');
        header.textContent = `Best Route: ${data.source} to ${data.destination}`
        resultElement.appendChild(header);
    
        data.flights.forEach(flight => {
          const card = createFlightCard(flight);
          resultElement.appendChild(card);
        });
    
        const summary = document.createElement('div');
        summary.className = 'summary';
        summary.innerHTML = `<strong>Total Price:</strong> $${data.total_cost} | <strong>Total Duration:</strong> ${Math.floor(data.total_duration / 60)} mins`;
        resultElement.appendChild(summary);}
        else{
          resultElement.textContent = "No routes found";}
  }catch(error){
    console.error(error);
    resultElement.textContent = "Failed to laod flights: " + error.message;
  } finally{
    loadingElement.style.display = 'none';
  }
  const end = performance.now();
  console.log(`Total Frontend-to-Backend-to-UI time: ${(end - start) / 1000}s`);
}

function formatDuration(seconds){
  minutes = seconds/60;
  const hours = Math.floor(minutes / 60);
  const mins = minutes % 60;
  const formattedMins = String(mins).padStart(2, '0');
  return `${hours}h ${formattedMins}m`;
}


function createFlightCard(flight){
  const durationText = formatDuration(flight.duration);
  const div = document.createElement('div');
  div.className = 'flight-card';

  div.innerHTML = `
    <div class = "airline-section">
      <span class="airline">Flight: ${flight.airline} ${flight.flight_no}</span>
    </div>
    
    <div class = "itinerary-section">
      <div class="timepoint">
        <span class = "airport">${flight.from}</span>
        <span class = "time">${flight.departure}</span>
      </div>
    
    <div class = "duration-line">
      <span class = "flight-duration"> ${durationText}</span>
      <div class = "line"></div>
    </div>

     <div class="timepoint">
        <span class = "airport">${flight.to}</span>
        <span class = "time">${flight.arrival}</span>
      </div>
    </div>
    <div class = "price-section">
      <div class="price">$${flight.price.toFixed(2)}</div>
    </div>`;
  return div;}


