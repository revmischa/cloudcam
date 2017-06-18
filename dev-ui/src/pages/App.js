import React from 'react'

import Navbar from '../components/Navbar'
import Notification from '../components/Notification'

const App = ({children}) => (
  <div id='app'>
    <Navbar />
    <main>
      {children}
    </main>
    <Notification />
  </div>
)

export default App
