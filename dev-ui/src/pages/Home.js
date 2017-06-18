import React from 'react'
import {connect} from 'react-redux'
import {refreshThings, requestThumbs} from '../api/iot'

class Home extends React.Component {
  constructor(props) {
    super(props)
    this.state = {
      things: [],
      user: false
    }
  }

  render() {
    const {things} = this.props
    return (
      <div>
        <section className='section content container'>
          <h2 className='title'>Things <a onClick={refreshThings}><i className='fa fa-refresh'
                                                                     aria-hidden='true'></i></a>
          </h2>
          <div>
            {Object.entries(things).length === 0 && <div>No things</div>}
            <ul>
              {Object.entries(things).map(function ([key, thing]) {
                return <li>
                  <div>
                    <p><img id={thing.name} className='thumb' src={thing.thumbUrl}/></p>
                    {thing.name}&nbsp;
                    <a onClick={() => requestThumbs([thing.name])}><i className='fa fa-refresh'
                                                                       aria-hidden='true'/></a>
                  </div>
                </li>
              })}
            </ul>
          </div>
        </section>
      </div>
    )
  }
}

const mapStateToProps = function (store) {
  return {
    things: store.iot.things,
    user: store.user.user
  }
};

export default connect(mapStateToProps)(Home)
