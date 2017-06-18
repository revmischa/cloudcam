import React from 'react'
import { Link } from 'react-router'
import { connect } from 'react-redux'

import { logout } from '../api/user'

const NavLink = ({to, children}) => (
  <Link activeClassName='is-active' className='nav-item is-tab' to={to}>
    {children}
  </Link>
)

const LinksDefault = ({className}) => (
  <div className={className}>
    <NavLink to='/register'>register</NavLink>
    <NavLink to='/login'>login</NavLink>
  </div>
)

const LinksAuth = ({user, className, onLogout}) => (
  <div className={className}>
    <NavLink to='/provision'>provision a thing</NavLink>
    <a className='nav-item is-tab' onClick={onLogout}>logout</a>
  </div>
)

class Navbar extends React.Component {
  constructor (props) {
    super(props)
    this.state = {
      user: false,
      open: false
    }
    this.menuToggle = this.menuToggle.bind(this)
    this.closeToggle = this.closeToggle.bind(this)
  }

  menuToggle (e) {
    e.stopPropagation()
    this.setState({open: !this.state.open})
  }

  closeToggle () {
    this.setState({open: false})
  }

  componentWillMount () {
    window.addEventListener('click', this.closeToggle, false)
  }

  componentWillUnmount () {
    window.removeEventListener('click', this.closeToggle, false)
  }

  render () {
    const {user} = this.props
    let menuClasses = 'nav-right nav-menu'
    if (this.state.open) {
      menuClasses += ' is-active'
    }
    return (
      <section className='Navbar hero is-info is-bold'>
        <nav className='nav'>
          <div className='nav-left'>
            <h1 className='site-logo nav-item'>
              <Link className='nav-item is-active' to='/'>
                <img src='/icon.svg' /> <span>Cloudcam</span>
              </Link>
            </h1>
          </div>
          <div className='nav-toggle'>
            <a className='nav-item' onClick={this.menuToggle}><i className='fa fa-bars' aria-hidden='true'></i></a>
          </div>
          {user ? <LinksAuth className={menuClasses} onLogout={logout} user={user} /> : <LinksDefault className={menuClasses} />}
        </nav>
      </section>
    )
  }
}

const mapStateToProps = function (store) {
  return {
    user: store.user.user
  }
}

export default connect(mapStateToProps)(Navbar)
