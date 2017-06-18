import React from 'react'

import { Link } from 'react-router'

const PageNotFound = (props) => (
  <div>
    <section className='hero is-danger is-medium is-bold'>
      <div className='hero-body container'>
        <h2 className='title'>404</h2>
        <h3 className='subtitle'>that page was not found.</h3>
      </div>
    </section>
    <section className='container is-medium section'>
      <h2 className='title'>sorry</h2>
      <h3 className='subtitle'>maybe try <Link to='/search'>searching</Link>?</h3>
    </section>
  </div>
)

PageNotFound.route = { path: '*', component: PageNotFound }

export default PageNotFound
